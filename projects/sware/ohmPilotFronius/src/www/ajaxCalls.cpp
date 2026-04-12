#include "ajaxCalls.h"

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

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
                                 DynamicJsonDocument &data,
                                 AsyncWebServerRequest *request);

static const char *safeJsonString(const JsonObject &obj, const char *key);
static void safeCopy(char *dest, size_t destSize, const char *src);

static void delayedRestartTask(void *parameter);
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

static void delayedRestartTask(void *parameter)
{
    const uint32_t delayMs = parameter == nullptr ? 1000U : *((uint32_t *)parameter);
    if (parameter != nullptr)
    {
        free(parameter);
    }

    LOG_INFO(TAG_AJAX, "delayedRestartTask - restart in %lu ms", static_cast<unsigned long>(delayMs));
    vTaskDelay(pdMS_TO_TICKS(delayMs));
    esp_restart();
}

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

    DynamicJsonDocument doc(SHELLY_JSON_BUFFER_LEN);
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

    DynamicJsonDocument data(JSON_OBJECT_SETUP_LEN);
    LOG_INFO(TAG_AJAX, "ajaxCalls_handleGetSetup - begin");

    data[WLAN_ESSID] = setup.ssid;
    data[WLAN_PASSWD] = setup.passwd;
    data[IP_INVERTER] = setup.inverter;

    data[HEIZSTABLEISTUNG] = setup.heizstab_leistung_in_watt;
    data[LEGIONELLEN_DELTA_TIME] = setup.legionellenDelta;
    data[LEGIONELLEN_TEMP] = setup.legionellenMaxTemp;

    data[EXTERNER_SPEICHER] = setup.externerSpeicher ? 0 : 1;

    data[EXTERNER_SPEICHER_PRIORI] = setup.externerSpeicherPriori;

    data[TEMP_AUSSCHALTEN] = setup.tempMaxAllowedInGrad; 
    data[TEMP_EINSCHALT] = setup.tempMinInGrad;

    data[WWW_MQTT_HOST] = setup.mqttHost;
    data[WWW_MQTT_USER] = setup.mqttUser;
    data[WWW_MQTT_PASWWD] = setup.mqttPass;

    data[WWW_INFLUX_HOST] = setup.influxHost;
    data[WWW_INFLUX_TOKEN] = setup.influxToken;
    data[WWW_INFLUX_ORG] = setup.influxOrg;
    data[WWW_INFLUX_BUCKET] = setup.influxBucket;

    data[AMIS_READER_HOST] = setup.amisReaderHost;
    data[AMIS_READER_KEY] = setup.amisKey;
    data[FORCE_HEIZPATRONE] = setup.forceHeating;
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
    data[WWW_AKKU] = webSockD.setupData.externerSpeicher;
    data[WWW_AKKU_KAPA] = webSockD.setupData.externerSpeicher ? webSockD.fronius_SOLAR_POWERFLOW.p_akku : 0.0;

    data[WWW_FLASH] = webSockD.states.flashOK;
    data[WWW_INFLUX] = webSockD.states.influx;
    data[WWW_INFLUX_IP] = webSockD.states.influx ? webSockD.setupData.influxHost : "";

    data[WWW_MODBUS] = webSockD.states.modbusOK;
    data[WWW_MODBUS_IP] = webSockD.states.modbusOK ? webSockD.setupData.inverter : "";

    data[WWW_MQTT] = webSockD.states.mqtt;
    data[WWW_MQTT_IP] = webSockD.states.mqtt ? webSockD.setupData.mqttHost : "";

    data[WWW_TEMP_SENSOR] = webSockD.states.tempSensorOK;
    data[WWW_EPSILON] = webSockD.setupData.epsilonML_PinManager;

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

/* ---------- store setup ---------- */

void ajaxCalls_handleStoreSetup(JsonDocument &json, AsyncWebServerRequest *request, bool isAPModus)
{
    ajaxCalls_ensureInitPrimitives();

    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(nullptr);
    LOG_INFO(TAG_AJAX, "ajaxCalls_handleStoreSetup - free stack words: %u",
             static_cast<unsigned>(stackRemaining));

    JsonObject jsonObj = json.as<JsonObject>();
    DynamicJsonDocument data(JSON_OBJECT_SETUP_LEN);
    //JsonObject ret = jsonObj.createNestedObject("data");
    Setup setup;
    memset(&setup, 0, sizeof(Setup));

    int result = 0;
    bool ok = false;
    const char *argument = nullptr;

    CALLBACK_SET_SETUP_CHANGED localSetSetupChanged = nullptr;

    if (!ajaxCalls_lock(g_ajaxMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
    {
        request->send(500, "application/json", "{\"done\":0,\"error\":\"mutex lock failed\"}");
        return;
    }

    localSetSetupChanged = g_setSetupChangedCallback;
    ajaxCalls_unlock(g_ajaxMutex);

    if (localSetSetupChanged != nullptr)
    {
        localSetSetupChanged(false);
    }

    argument = safeJsonString(jsonObj, WLAN_ESSID);
    ok = util_isFieldFilled(WLAN_ESSID, argument, data);
    if (!ok)
    {
        returnFromStoreSetup(false, data, request);
        return;
    }
    safeCopy(setup.ssid, LEN_WLAN, argument);

    argument = safeJsonString(jsonObj, WLAN_PASSWD);
    ok = util_isFieldFilled(WLAN_PASSWD, argument, data);
    if (!ok)
    {
        returnFromStoreSetup(false, data, request);
        return;
    }
    safeCopy(setup.passwd, LEN_WLAN, argument);

    argument = safeJsonString(jsonObj, IP_INVERTER);
    ok = util_isFieldFilled(IP_INVERTER, argument, data);
    if (!ok)
    {
        returnFromStoreSetup(false, data, request);
        return;
    }
    safeCopy(setup.inverter, INET_ADDRSTRLEN, argument);

    argument = safeJsonString(jsonObj, HEIZSTABLEISTUNG);

    if (argument == "")
    {
        if (jsonObj["heizstab_leistung"].is<int>())
        {
            setup.heizstab_leistung_in_watt = jsonObj["heizstab_leistung"].as<int>();
        }
    }
    else
    {
        ok = util_checkParamInt(HEIZSTABLEISTUNG, argument, data, &result);
        if (!ok)
        {
            LOG_ERROR(TAG_AJAX, "===> Invalid value for heizstab_leistung_in_watt: '%s'", argument);
            returnFromStoreSetup(false, data, request);
            return;
        }
        setup.heizstab_leistung_in_watt = result;
    }

    argument = safeJsonString(jsonObj, LEGIONELLEN_DELTA_TIME);
    if (argument == "")
    {
        if (jsonObj[LEGIONELLEN_DELTA_TIME].is<int>())
        {
            setup.legionellenDelta = jsonObj[LEGIONELLEN_DELTA_TIME].as<int>();
        }
    }
    else
    {
        ok = util_checkParamInt(LEGIONELLEN_DELTA_TIME, argument, data, &result);
        if (!ok)
        {
            returnFromStoreSetup(false, data, request);
            return;
        }
        setup.legionellenDelta = result;
    }
    //!!setup.pid_powerWhichNeedNotConsumed = result;

    argument = safeJsonString(jsonObj, LEGIONELLEN_TEMP);
    if (argument == "")
    {
        if (jsonObj[LEGIONELLEN_TEMP].is<int>())
        {
            setup.legionellenMaxTemp = jsonObj[LEGIONELLEN_TEMP].as<int>();
        }
    }
    else
    {
        ok = util_checkParamInt(LEGIONELLEN_TEMP, argument, data, &result);
        if (!ok)
        {
            returnFromStoreSetup(false, data, request);
            return;
        }
        setup.legionellenMaxTemp = result;
    }

    argument = safeJsonString(jsonObj, EXTERNER_SPEICHER);
    if (argument == "")
    {
        if (jsonObj[EXTERNER_SPEICHER].is<int>())
        {
            setup.externerSpeicher = jsonObj[EXTERNER_SPEICHER].as<int>();
        }
    }
    else
    {
        setup.externerSpeicher = (argument[0] == 'j' || argument[0] == 'J');
    }

    argument = safeJsonString(jsonObj, EXTERNER_SPEICHER_PRIORI);
    if (argument == "")
    {
        if (jsonObj[EXTERNER_SPEICHER_PRIORI].is<int>())
        {
            setup.externerSpeicherPriori = jsonObj[EXTERNER_SPEICHER_PRIORI].as<int>();
        }
    }
    else
    {

        ok = util_isFieldFilled(EXTERNER_SPEICHER_PRIORI, argument, data);
        if (!ok)
        {
            returnFromStoreSetup(false, data, request);
            return;
        }
        setup.externerSpeicherPriori = argument[0];
    }
 
    argument = safeJsonString(jsonObj, TEMP_AUSSCHALTEN);
    if (argument == "")
    {
        if (jsonObj[TEMP_AUSSCHALTEN].is<int>())
        {
            setup.tempMaxAllowedInGrad = jsonObj[TEMP_AUSSCHALTEN].as<int>();
        }
    }
    else
    {

        ok = util_checkParamInt(TEMP_AUSSCHALTEN, argument, data, &result);
        if (!ok)
        {
            returnFromStoreSetup(false, data, request);
            return;
        }
        setup.tempMaxAllowedInGrad = result;
    }

    argument = safeJsonString(jsonObj, TEMP_EINSCHALT);
    if (argument == "")
    {
        if (jsonObj[TEMP_EINSCHALT].is<int>())
        {
            setup.tempMinInGrad = jsonObj[TEMP_EINSCHALT].as<int>();
        }
    }
    else
    {

        ok = util_checkParamInt(TEMP_EINSCHALT, argument, data, &result);
        if (!ok)
        {
            returnFromStoreSetup(false, data, request);
            return;
        }
        setup.tempMinInGrad = result;
    }
    argument = safeJsonString(jsonObj, WWW_MQTT_HOST);
    if (strlen(argument) < 3)
    {
        argument = EMPTY_STRING;
    }
    safeCopy(setup.mqttHost, MQTT_HOST_LEN, argument);

    argument = safeJsonString(jsonObj, WWW_MQTT_PASWWD);
    if (strlen(argument) < 3)
    {
        argument = EMPTY_STRING;
    }
    safeCopy(setup.mqttPass, MQTT_PASS_LEN, argument);

    argument = safeJsonString(jsonObj, WWW_MQTT_USER);
    if (strlen(argument) < 3)
    {
        argument = EMPTY_STRING;
    }
    safeCopy(setup.mqttUser, MQTT_USER_LEN, argument);

    argument = safeJsonString(jsonObj, WWW_INFLUX_HOST);
    if (strlen(argument) < 3)
    {
        argument = EMPTY_STRING;
    }
    safeCopy(setup.influxHost, INFLUX_HOST_LEN, argument);

    argument = safeJsonString(jsonObj, WWW_INFLUX_TOKEN);
    if (strlen(argument) < 3)
    {
        argument = EMPTY_STRING;
    }
    safeCopy(setup.influxToken, INFLUX_TOKEN_LEN, argument);

    argument = safeJsonString(jsonObj, WWW_INFLUX_ORG);
    if (strlen(argument) < 3)
    {
        argument = EMPTY_STRING;
    }
    safeCopy(setup.influxOrg, INFLUX_ORG_LEN, argument);

    argument = safeJsonString(jsonObj, WWW_INFLUX_BUCKET);
    if (strlen(argument) < 3)
    {
        argument = EMPTY_STRING;
    }
    safeCopy(setup.influxBucket, INFLUX_BUCKET_LEN, argument);

    argument = safeJsonString(jsonObj, AMIS_READER_HOST);
    if (strlen(argument) < 3)
    {
        argument = EMPTY_STRING;
    }
    safeCopy(setup.amisReaderHost, INET_ADDRSTRLEN, argument);

    argument = safeJsonString(jsonObj, AMIS_READER_KEY);
    ok = util_isFieldFilled(AMIS_READER_KEY, argument, data);
    if (!ok)
    {
        returnFromStoreSetup(false, data, request);
        return;
    }
    safeCopy(setup.amisKey, AMIS_KEY_LEN, argument);

    argument = safeJsonString(jsonObj, FORCE_HEIZPATRONE);
    if (argument == "")
    {
        if (jsonObj[FORCE_HEIZPATRONE].is<int>())
        {
            setup.forceHeating = jsonObj[FORCE_HEIZPATRONE].as<int>();
        }
    }
    else
    {

        if (strlen(argument) > 0)
        {
            if (util_checkParamInt(FORCE_HEIZPATRONE, argument, data, &result))
            {
                setup.forceHeating = result;
            }
        }
    }

    if (localSetSetupChanged != nullptr)
    {
        localSetSetupChanged(true);
    }

    eprom_storeSetup(setup);
    eprom_test_read_Eprom();

    returnFromStoreSetup(true, data, request);

    if (isAPModus)
    {
        uint32_t *delayMs = static_cast<uint32_t *>(malloc(sizeof(uint32_t)));
        if (delayMs != nullptr)
        {
            *delayMs = 10000U;
            BaseType_t taskOk =
                xTaskCreatePinnedToCore(
                    delayedRestartTask,
                    "ajaxRestartTask",
                    2048,
                    delayMs,
                    1,
                    nullptr,
                    tskNO_AFFINITY);

            if (taskOk != pdPASS)
            {
                free(delayMs);
                LOG_ERROR(TAG_AJAX, "ajaxCalls_handleStoreSetup - restart task creation failed");
            }
        }
    }
}

/* ---------- response helper ---------- */

static void returnFromStoreSetup(bool inputCorrect,
                                 DynamicJsonDocument &data,
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