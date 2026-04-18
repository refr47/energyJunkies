#include "ajaxCalls.h"

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "app_tasks.h"

#include "eprom.h"
#include "utils.h"
#include "shelly.h"

#ifndef SHELLY_SCAN_MAX_DEVICES
#define SHELLY_SCAN_MAX_DEVICES 254
#endif

#ifndef SHELLY_JSON_BUFFER_LEN
#define SHELLY_JSON_BUFFER_LEN 8192
#endif

#ifndef AJAX_MUTEX_TIMEOUT_MS
#define AJAX_MUTEX_TIMEOUT_MS 2000
#endif

enum FieldType
{
    TYPE_STRING,
    TYPE_UINT,
    TYPE_INT,
    TYPE_SHORT,
    TYPE_BOOL,
    TYPE_DOUBLE
};
struct FieldDescriptor
{
    const char *key;
    FieldType type;
    size_t offset;
    size_t maxLen;
    bool required;
};

static CALLBACK_GET_DATA g_getDataCallback = nullptr;
static CALLBACK_SET_SETUP_CHANGED g_setSetupChangedCallback = nullptr;

static SemaphoreHandle_t g_ajaxMutex = nullptr;
static SemaphoreHandle_t g_shellyMutex = nullptr;

static TaskHandle_t g_shellyTaskHandle = nullptr;
static volatile bool g_shellyScanRunning = false;
static volatile bool g_shellyScanDone = false;

/* letzter Snapshot des Shelly-Scans */
static char g_shellyJsonCache[SHELLY_JSON_BUFFER_LEN] = {0};

/* ---------- private helpers ---------- */

static void ajaxCalls_ensureInitPrimitives();
static bool ajaxCalls_lock(SemaphoreHandle_t mutex, TickType_t timeoutTicks);
static void ajaxCalls_unlock(SemaphoreHandle_t mutex);

static void returnFromStoreSetup(bool inputCorrect,
                                 JsonDocument &data,
                                 AsyncWebServerRequest *request);

void delayedRestartTask(void *param);
bool parseStruct(JsonObject obj,
                 void *structPtr,
                 const FieldDescriptor *fields,
                 size_t fieldCount,
                 JsonDocument &response);

static const char *safeJsonString(const JsonObject &obj, const char *key);
static void safeCopy(char *dest, size_t destSize, const char *src);

static void shellyScanTask(void *parameter);

static void fillShellyJsonObj(const ALL_SHELLY_DEVICES &data, JsonArray &array);
static void fillShellyJsonObjWithErrorMsg(const ALL_SHELLY_DEVICES &data, JsonArray &array);

/* ---------- init ---------- */

void ajaxCalls_init(CALLBACK_GET_DATA getData, CALLBACK_SET_SETUP_CHANGED setupCh)
{
    ajaxCalls_ensureInitPrimitives();

    if (!ajaxCalls_lock(g_ajaxMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
    {
        LOG_ERROR(TAG_AJAX, "ajaxCalls_init - mutex lock failed");
        return;
    }

    g_getDataCallback = getData;
    g_setSetupChangedCallback = setupCh;

    ajaxCalls_unlock(g_ajaxMutex);
}

static void ajaxCalls_ensureInitPrimitives()
{
    if (g_ajaxMutex == nullptr)
    {
        g_ajaxMutex = xSemaphoreCreateMutex();
    }

    if (g_shellyMutex == nullptr)
    {
        g_shellyMutex = xSemaphoreCreateMutex();
    }

    if (g_shellyJsonCache[0] == '\0')
    {
        strncpy(g_shellyJsonCache,
                "{\"done\":0,\"error\":\"scan not started\",\"DATA\":[]}",
                sizeof(g_shellyJsonCache) - 1);
        g_shellyJsonCache[sizeof(g_shellyJsonCache) - 1] = '\0';
    }
}

static bool ajaxCalls_lock(SemaphoreHandle_t mutex, TickType_t timeoutTicks)
{
    if (mutex == nullptr)
    {
        return false;
    }
    return xSemaphoreTake(mutex, timeoutTicks) == pdTRUE;
}

static void ajaxCalls_unlock(SemaphoreHandle_t mutex)
{
    if (mutex != nullptr)
    {
        xSemaphoreGive(mutex);
    }
}

static bool updateIntFromStore(JsonDocument &doc, const char *key, int &targetValue)
{
    if (!doc.containsKey(key))
        return true; // Optional: ignorieren wenn Feld fehlt

    JsonVariant v = doc[key];

    // Versuch die Zahl zu extrahieren (egal ob "123" oder 123)
    if (v.is<int>() || v.is<float>())
    {
        targetValue = v.as<int>();
        return true;
    }

    return false;
}

/* ---------- helpers ---------- */

static const char *safeJsonString(const JsonObject &obj, const char *key)
{
    return obj[key] | "";
}

static void safeCopy(char *dest, size_t destSize, const char *src)
{
    if (dest == nullptr || destSize == 0)
    {
        return;
    }

    if (src == nullptr)
    {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

static void fillShellyJsonObj(const ALL_SHELLY_DEVICES &data, JsonArray &array)
{
    if (data.shellyDevice == nullptr)
    {
        return;
    }

    JsonObject object1 = array.createNestedObject();
    object1["IP"] = data.shellyDevice->ip;
    object1["PORT"] = data.shellyDevice->port;
    object1["NAME"] = data.shellyDevice->name;
}

static void fillShellyJsonObjWithErrorMsg(const ALL_SHELLY_DEVICES &data, JsonArray &array)
{
    if (data.errorContainer == nullptr)
    {
        return;
    }

    JsonObject object1 = array.createNestedObject();
    object1["ERROR"] = data.errorContainer->errorMessage;
}

/* ---------- restart task ---------- */

/* ---------- shelly scan ---------- */

bool ajaxCalls_triggerShellyScan(void)
{
    ajaxCalls_ensureInitPrimitives();

    if (!ajaxCalls_lock(g_shellyMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
    {
        LOG_ERROR(TAG_AJAX, "ajaxCalls_triggerShellyScan - mutex lock failed");
        return false;
    }

    if (g_shellyScanRunning)
    {
        ajaxCalls_unlock(g_shellyMutex);
        LOG_INFO(TAG_AJAX, "ajaxCalls_triggerShellyScan - scan already running");
        return false;
    }

    g_shellyScanRunning = true;
    g_shellyScanDone = false;

    bool created =
        xTaskCreatePinnedToCore(
            shellyScanTask,
            "shellyScanTask",
            8192,
            nullptr,
            1,
            &g_shellyTaskHandle,
            tskNO_AFFINITY) == pdPASS;

    if (!created)
    {
        g_shellyScanRunning = false;
        strncpy(g_shellyJsonCache,
                "{\"done\":0,\"error\":\"cannot create shelly task\",\"DATA\":[]}",
                sizeof(g_shellyJsonCache) - 1);
        g_shellyJsonCache[sizeof(g_shellyJsonCache) - 1] = '\0';
        LOG_ERROR(TAG_AJAX, "ajaxCalls_triggerShellyScan - task creation failed");
    }

    ajaxCalls_unlock(g_shellyMutex);
    return created;
}

static void shellyScanTask(void *parameter)
{
    (void)parameter;
    ajaxCalls_ensureInitPrimitives();

    JsonDocument doc;
    JsonArray array = doc["DATA"].to<JsonArray>();
    doc["done"] = 0;
    doc["error"] = "";

    ALL_SHELLY_DEVICES *allDevices =
        static_cast<ALL_SHELLY_DEVICES *>(calloc(SHELLY_SCAN_MAX_DEVICES, sizeof(ALL_SHELLY_DEVICES)));

    if (allDevices == nullptr)
    {
        doc["done"] = 0;
        doc["error"] = "No memory left";

        String response;
        serializeJson(doc, response);

        if (ajaxCalls_lock(g_shellyMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
        {
            safeCopy(g_shellyJsonCache, sizeof(g_shellyJsonCache), response.c_str());
            g_shellyScanRunning = false;
            g_shellyScanDone = true;
            g_shellyTaskHandle = nullptr;
            ajaxCalls_unlock(g_shellyMutex);
        }

        vTaskDelete(nullptr);
        return;
    }

    char ipVek[INET_ADDRSTRLEN] = {0};
    String s = WiFi.localIP().toString();
    safeCopy(ipVek, sizeof(ipVek), s.c_str());

    char *cp = strrchr(ipVek, '.');
    if (cp == nullptr)
    {
        doc["done"] = 0;
        doc["error"] = "Internal Error";

        String response;
        serializeJson(doc, response);

        free(allDevices);

        if (ajaxCalls_lock(g_shellyMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
        {
            safeCopy(g_shellyJsonCache, sizeof(g_shellyJsonCache), response.c_str());
            g_shellyScanRunning = false;
            g_shellyScanDone = true;
            g_shellyTaskHandle = nullptr;
            ajaxCalls_unlock(g_shellyMutex);
        }

        vTaskDelete(nullptr);
        return;
    }

    *cp = '\0';
    char *range = ipVek;

    bool listResult = shelly_listAllDevices(allDevices, range, SHELLY_SCAN_MAX_DEVICES);
    if (!listResult)
    {
        LOG_WARNING(TAG_AJAX, "shellyScanTask - shelly_listAllDevices returned false");
    }

    for (int jj = 0; jj < SHELLY_SCAN_MAX_DEVICES; ++jj)
    {
        if (allDevices[jj].valid)
        {
            fillShellyJsonObj(allDevices[jj], array);

            if (allDevices[jj].shellyDevice != nullptr)
            {
                free(allDevices[jj].shellyDevice);
                allDevices[jj].shellyDevice = nullptr;
            }
        }
        else if (allDevices[jj].errorContainer != nullptr)
        {
            fillShellyJsonObjWithErrorMsg(allDevices[jj], array);
            free(allDevices[jj].errorContainer);
            allDevices[jj].errorContainer = nullptr;
        }
    }

    free(allDevices);

    doc["done"] = 1;
    if (!listResult)
    {
        doc["error"] = "scan finished with warnings";
    }

    String response;
    serializeJson(doc, response);

    if (ajaxCalls_lock(g_shellyMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
    {
        safeCopy(g_shellyJsonCache, sizeof(g_shellyJsonCache), response.c_str());
        g_shellyScanRunning = false;
        g_shellyScanDone = true;
        g_shellyTaskHandle = nullptr;
        ajaxCalls_unlock(g_shellyMutex);
    }

    LOG_INFO(TAG_AJAX, "shellyScanTask - finished");
    vTaskDelete(nullptr);
}

void ajaxCalls_handleBuildAndGetShelly(AsyncWebServerRequest *request)
{
    ajaxCalls_ensureInitPrimitives();

    char response[SHELLY_JSON_BUFFER_LEN];

    if (!ajaxCalls_lock(g_shellyMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
    {
        request->send(500, "application/json", "{\"done\":0,\"error\":\"mutex lock failed\",\"DATA\":[]}");
        return;
    }

    safeCopy(response, sizeof(response), g_shellyJsonCache);

    if (g_shellyScanRunning)
    {
        ajaxCalls_unlock(g_shellyMutex);
        request->send(200, "application/json",
                      "{\"done\":0,\"error\":\"scan running\",\"DATA\":[]}");
        return;
    }

    ajaxCalls_unlock(g_shellyMutex);
    request->send(200, "application/json", response);
}

/* ---------- get setup ---------- */

void ajaxCalls_handleGetSetup(AsyncWebServerRequest *request)
{
    Setup setup;
    eprom_getSetup(setup);

    JsonDocument data;
    LOG_INFO(TAG_AJAX, "ajaxCalls_handleGetSetup - begin");

    data[WLAN_ESSID] = setup.ssid;
    data[WLAN_PASSWD] = setup.passwd;
    data[IP_INVERTER] = setup.inverter;
    data[AMIS_READER_HOST] = setup.amisReaderHost;
    data[AMIS_READER_KEY] = setup.amisKey;

    data[FORCE_HEIZPATRONE] = setup.forceHeating;
    data[HEIZSTABLEISTUNG] = setup.heizstab_leistung_in_watt;
    data[TEMP_AUSSCHALTEN] = setup.tempMaxAllowedInGrad;
    data[TEMP_EINSCHALT] = setup.tempMinInGrad;

    data[LEGIONELLEN_DELTA_TIME] = setup.legionellenDelta;
    data[LEGIONELLEN_TEMP] = setup.legionellenMaxTemp;

    data[AKKU] = setup.akku;
    data[AKKU_PRIORI] = setup.akkuPriori;

    data[WWW_MQTT_HOST] = setup.mqttHost;
    data[WWW_MQTT_USER] = setup.mqttUser;
    data[WWW_MQTT_PASWWD] = setup.mqttPass;

    data[WWW_INFLUX_HOST] = setup.influxHost;
    data[WWW_INFLUX_TOKEN] = setup.influxToken;
    data[WWW_INFLUX_ORG] = setup.influxOrg;
    data[WWW_INFLUX_BUCKET] = setup.influxBucket;
    data[PID_EPSILON] = setup.epsilonML_PinManager;

    data[WWW_WATT_BIAS] = setup.wattSetupForTest;
    String response;
    serializeJson(data, response);
    LOG_INFO(TAG_AJAX, "Send AJAX Data %s", response.c_str());
    request->send(200, "application/json", response);
    // returnFromStoreSetup(true, data, request);
}

/* ---------- overview ---------- */

void ajaxCalls_handleGetOverview(AsyncWebServerRequest *request)
{
    ajaxCalls_ensureInitPrimitives();

    if (!ajaxCalls_lock(g_ajaxMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
    {
        request->send(500, "application/json", "{\"done\":0,\"error\":\"mutex lock failed\"}");
        return;
    }

    CALLBACK_GET_DATA localGetData = g_getDataCallback;
    ajaxCalls_unlock(g_ajaxMutex);

    if (localGetData == nullptr)
    {
        request->send(500, "application/json", "{\"done\":0,\"error\":\"callback not initialized\"}");
        return;
    }

    WEBSOCK_DATA webSockD = localGetData();

    DynamicJsonDocument data(JSON_OBJECT_SETUP_LEN);
    char formatBuffer[100] = {0};

    data[WWW_FRONIUS] = webSockD.states.froniusAPI;
    data[WWW_FRONIUS_IP] = webSockD.states.froniusAPI ? webSockD.setupData.inverter : "";

    data[WWW_AMIS] = webSockD.states.amisReader;
    data[WWW_AMIS_IP] = webSockD.states.amisReader ? webSockD.setupData.amisReaderHost : "";

    data[WWW_CARDREADER] = webSockD.states.cardWriterOK;
    data[WWW_AKKU] = webSockD.setupData.akku;
    data[WWW_AKKU_KAPA] = webSockD.setupData.akku ? webSockD.fronius_SOLAR_POWERFLOW.p_akku : 0.0;

    data[WWW_FLASH] = webSockD.states.flashOK;
    data[WWW_INFLUX] = webSockD.states.influx;
    data[WWW_INFLUX_IP] = webSockD.states.influx ? webSockD.setupData.influxHost : "";

    data[WWW_MODBUS] = webSockD.states.modbusOK;
    data[WWW_MODBUS_IP] = webSockD.states.modbusOK ? webSockD.setupData.inverter : "";

    data[WWW_MQTT] = webSockD.states.mqtt;
    data[WWW_MQTT_IP] = webSockD.states.mqtt ? webSockD.setupData.mqttHost : "";

    data[WWW_TEMP_SENSOR] = webSockD.states.tempSensorOK;
    data[WWW_EPSILON] = webSockD.setupData.epsilonML_PinManager;
    data[WWW_WATT_BIAS] = webSockD.setupData.wattSetupForTest;
    if (webSockD.temperature.alarm)
    {
        if (webSockD.temperature.sensor1 > 0 && webSockD.temperature.sensor2 > 0)
        {
            snprintf(formatBuffer,
                     sizeof(formatBuffer),
                     "!!%.2f!!",
                     (webSockD.temperature.sensor1 + webSockD.temperature.sensor2) / 2.0);
        }
        else
        {
            snprintf(formatBuffer,
                     sizeof(formatBuffer),
                     "!!%.2f %.2f!!",
                     webSockD.temperature.sensor1,
                     webSockD.temperature.sensor2);
        }
    }
    else
    {
        snprintf(formatBuffer,
                 sizeof(formatBuffer),
                 "%.2f",
                 (webSockD.temperature.sensor1 + webSockD.temperature.sensor2) / 2.0);
    }

    data[WWW_TEMP_SENSOR_VAL] = formatBuffer;
    data["done"] = 1;
    data["error"] = "";

    String response;
    serializeJson(data, response);
    request->send(200, "application/json", response);
}

/* ---------- store setup -----
 *********************************************************************
 */

/* const FieldDescriptor setupFields[] =
    {
        {"ssid", TYPE_STRING, OFFSET(Setup, ssid), LEN_WLAN + 1, true},
        {"passwd", TYPE_STRING, OFFSET(Setup, passwd), LEN_WLAN + 1, true},

        {"heizstab_leistung_in_watt", TYPE_UINT, OFFSET(Setup, heizstab_leistung_in_watt), 0, true},
        {"phasen_leistung_in_watt", TYPE_UINT, OFFSET(Setup, phasen_leistung_in_watt), 0, true},
        {"tempMaxAllowedInGrad", TYPE_UINT, OFFSET(Setup, tempMaxAllowedInGrad), 0, true},
        {"tempMinInGrad", TYPE_UINT, OFFSET(Setup, tempMinInGrad), 0, true},

        {"inverter", TYPE_STRING, OFFSET(Setup, inverter), INET_ADDRSTRLEN + 1, false},
        {"currentIP", TYPE_STRING, OFFSET(Setup, currentIP), INET_ADDRSTRLEN + 1, false},

        {"externerSpeicher", TYPE_BOOL, OFFSET(Setup, externerSpeicher), 0, false},
        {"externerSpeicherPriori", TYPE_SHORT, OFFSET(Setup, externerSpeicherPriori), 0, false},

        {"legionellenDelta", TYPE_UINT, OFFSET(Setup, legionellenDelta), 0, false},
        {"legionellenMaxTemp", TYPE_UINT, OFFSET(Setup, legionellenMaxTemp), 0, false},

        {"amisKey", TYPE_STRING, OFFSET(Setup, amisKey), AMIS_KEY_LEN + 1, false},
        {"amisReaderHost", TYPE_STRING, OFFSET(Setup, amisReaderHost), INET_ADDRSTRLEN + 1, false},

        {"mqttHost", TYPE_STRING, OFFSET(Setup, mqttHost), MQTT_HOST_LEN + 1, false},
        {"mqttUser", TYPE_STRING, OFFSET(Setup, mqttUser), MQTT_USER_LEN + 1, false},
        {"mqttPass", TYPE_STRING, OFFSET(Setup, mqttPass), MQTT_PASS_LEN + 1, false},

        {"influxHost", TYPE_STRING, OFFSET(Setup, influxHost), INFLUX_HOST_LEN + 1, false},
        {"influxToken", TYPE_STRING, OFFSET(Setup, influxToken), INFLUX_TOKEN_LEN + 1, false},
        {"influxOrg", TYPE_STRING, OFFSET(Setup, influxOrg), INFLUX_ORG_LEN + 1, false},
        {"influxBucket", TYPE_STRING, OFFSET(Setup, influxBucket), INFLUX_BUCKET_LEN + 1, false},

        {"epsilonML_PinManager", TYPE_DOUBLE, OFFSET(Setup, epsilonML_PinManager), 0, false},

        {"forceHeating", TYPE_INT, OFFSET(Setup, forceHeating), 0, false},
        {"wattSetupForTest", TYPE_INT, OFFSET(Setup, wattSetupForTest), 0, false},

        {"setupChanged", TYPE_BOOL, OFFSET(Setup, setupChanged), 0, false}}
; */

template <typename T, typename Struct>
struct Field
{
    const char *key;
    T Struct::*member;
    size_t maxLen;
    bool required;
};

template <typename Struct, size_t N>
struct StringField
{
    const char *key;
    char (Struct::*member)[N];
    bool required;
};

template <typename T, typename Struct>
bool parseField(JsonObject obj,
                const Field<T, Struct> &f,
                Struct &s,
                JsonDocument &res)
{
    if (!obj.containsKey(f.key))
    {
        if (f.required)
        {
            res["error"] = String("Missing: ") + f.key;
            return false;
        }
        return true;
    }

    JsonVariant v = obj[f.key];

    if (!v.is<T>())
    {
        res["error"] = String("Invalid: ") + f.key;
        return false;
    }

    s.*(f.member) = v.as<T>();
    return true;
}

template <typename Struct, size_t N>
bool parseField(JsonObject obj,
                const StringField<Struct, N> &f,
                Struct &s,
                JsonDocument &res)
{
    const char *value = obj[f.key];

    if (!value || strlen(value) == 0)
    {
        if (f.required)
        {
            res["error"] = String("Missing: ") + f.key;
            return false;
        }
        return true;
    }

    char *target = (s.*(f.member));
    strncpy(target, value, N - 1);
    target[N - 1] = '\0';

    return true;
}

template <typename Struct, typename... Fields>
bool parseStruct(JsonObject obj,
                 Struct &s,
                 JsonDocument &res,
                 Fields... fields)
{
    bool ok = true;

    (void)std::initializer_list<int>{
        (ok = ok && parseField(obj, fields, s, res), 0)...};

    return ok;
}

void ajaxCalls_handleStoreSetup(JsonDocument &json,
                                AsyncWebServerRequest *request,
                                bool isAPModus)
{
    ajaxCalls_ensureInitPrimitives();

    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(nullptr);
    LOG_INFO(TAG_AJAX, "free stack words: %u", (unsigned)stackRemaining);

    JsonObject jsonObj = json.as<JsonObject>();
    JsonDocument data;

    CALLBACK_SET_SETUP_CHANGED cb = nullptr;

    if (!ajaxCalls_lock(g_ajaxMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
    {
        request->send(500, "application/json",
                      "{\"done\":0,\"error\":\"mutex lock failed\"}");
        return;
    }

    cb = g_setSetupChangedCallback;
    ajaxCalls_unlock(g_ajaxMutex);

    if (cb)
        cb(false);

    Setup setup{};

    bool ok = parseStruct(
        jsonObj,
        setup,
        data,

        StringField<Setup, LEN_WLAN + 1>{WLAN_ESSID, &Setup::ssid, true},
        StringField<Setup, LEN_WLAN + 1>{WLAN_PASSWD, &Setup::passwd, true},
        StringField<Setup, INET_ADDRSTRLEN + 1>{IP_INVERTER, &Setup::inverter, true},

        StringField<Setup, INET_ADDRSTRLEN + 1>{AMIS_READER_HOST, &Setup::amisReaderHost, false},
        StringField<Setup, AMIS_KEY_LEN + 1>{AMIS_READER_KEY, &Setup::amisKey, false},

        Field<unsigned int, Setup>{HEIZSTABLEISTUNG, &Setup::heizstab_leistung_in_watt, 0, true},
        Field<int, Setup>{FORCE_HEIZPATRONE, &Setup::forceHeating, 0, false},
        Field<unsigned int, Setup>{TEMP_AUSSCHALTEN, &Setup::tempMaxAllowedInGrad, 0, true},
        Field<unsigned int, Setup>{TEMP_EINSCHALT, &Setup::tempMinInGrad, 0, true},

        Field<unsigned int, Setup>{LEGIONELLEN_DELTA_TIME, &Setup::legionellenDelta, 0, false},
        Field<unsigned int, Setup>{LEGIONELLEN_TEMP, &Setup::legionellenMaxTemp, 0, false},

        Field<short, Setup>{AKKU, &Setup::akku, 0, false},
        Field<short, Setup>{AKKU_PRIORI, &Setup::akkuPriori, 0, false},

        StringField<Setup, MQTT_HOST_LEN + 1>{WWW_MQTT_HOST, &Setup::mqttHost, false},
        StringField<Setup, MQTT_USER_LEN + 1>{WWW_MQTT_USER, &Setup::mqttUser, false},
        StringField<Setup, MQTT_PASS_LEN + 1>{WWW_MQTT_PASWWD, &Setup::mqttPass, false},
        StringField<Setup, INFLUX_HOST_LEN + 1>{WWW_INFLUX_HOST, &Setup::influxHost, false},
        StringField<Setup, INFLUX_TOKEN_LEN + 1>{WWW_INFLUX_TOKEN, &Setup::influxToken, false},
        StringField<Setup, INFLUX_ORG_LEN + 1>{WWW_INFLUX_ORG, &Setup::influxOrg, false},
        StringField<Setup, INFLUX_BUCKET_LEN + 1>{WWW_INFLUX_BUCKET, &Setup::influxBucket, false},
        Field<double, Setup>{PID_EPSILON, &Setup::epsilonML_PinManager, 0, false},
        Field<int, Setup>{WWW_WATT_BIAS, &Setup::wattSetupForTest, 0, false});

    setup.phasen_leistung_in_watt = setup.heizstab_leistung_in_watt / 3;

    if (!ok)
    {
        returnFromStoreSetup(false, data, request);
        return;
    }

    if (cb)
        cb(true);

    // eprom_storeSetup(setup);
    if (!appTask_epromWriter(&setup))
    {
        data["error"] = "queue full";
        returnFromStoreSetup(false, data, request);
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }
    returnFromStoreSetup(true, data, request);

    if (isAPModus)
    {
        static uint32_t delayMs = 10000U;

        BaseType_t taskOk = xTaskCreatePinnedToCore(
            delayedRestartTask,
            "ajaxRestartTask",
            2048,
            &delayMs,
            1,
            nullptr,
            tskNO_AFFINITY);

        if (taskOk != pdPASS)
        {
            LOG_ERROR(TAG_AJAX, "restart task creation failed");
        }
    }
}

void delayedRestartTask(void *param)
{
    uint32_t delayMs = (uint32_t)param;

    vTaskDelay(pdMS_TO_TICKS(delayMs));

    esp_restart();

    vTaskDelete(nullptr);
}

/* ---------- response helper ---------- */

static void returnFromStoreSetup(bool inputCorrect,
                                 JsonDocument &data,
                                 AsyncWebServerRequest *request)
{
    String response;

    if (inputCorrect)
    {
        data["done"] = 1;
        data["error"] = "";
        LOG_INFO(TAG_AJAX, "returnFromStoreSetup - no errors");
    }
    else
    {
        data["done"] = 0;
        if (!data.containsKey("error"))
        {
            data["error"] = "invalid input";
        }
        LOG_ERROR(TAG_AJAX, "returnFromStoreSetup - errors");
    }
    LOG_INFO(TAG_AJAX, "Send AJAX Data %s", response.c_str());

    serializeJson(data, response);
    request->send(200, "application/json", response);
}