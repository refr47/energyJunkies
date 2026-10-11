#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>
#include "app_tasks.h"

#include "eprom.h"
#include "utils.h"
#include "shelly.h"
#include "templ.h"
#include "ajaxConst.h"
#include "setupFields.h"
#include "ajaxCalls.h"
#ifndef SHELLY_SCAN_MAX_DEVICES
#define SHELLY_SCAN_MAX_DEVICES 254
#endif

#ifndef SHELLY_JSON_BUFFER_LEN
#define SHELLY_JSON_BUFFER_LEN 8192
#endif

#ifndef AJAX_MUTEX_TIMEOUT_MS
#define AJAX_MUTEX_TIMEOUT_MS 2000
#endif

CALLBACK_GET_DATA g_getDataCallback = nullptr;
static CALLBACK_SET_SETUP_CHANGED g_setSetupChangedCallback = nullptr;

// Mutexes werden zentral durch appSyncInit() erstellt → extern über ajaxCalls.h
// extern SemaphoreHandle_t g_ajaxMutex;
// extern SemaphoreHandle_t g_shellyMutex;

static TaskHandle_t g_shellyTaskHandle = nullptr;
bool g_shellyScanRunning{false};
static std::atomic<bool> g_shellyScanDone{false};

/* letzter Snapshot des Shelly-Scans */
static char g_shellyJsonCache[SHELLY_JSON_BUFFER_LEN] = {0};

/* ---------- private helpers ---------- */

static bool ajaxCalls_lock(SemaphoreHandle_t mutex, TickType_t timeoutTicks);
static void ajaxCalls_unlock(SemaphoreHandle_t mutex);

static void returnFromStoreSetup(bool inputCorrect,
                                 JsonDocument &result,
                                 AsyncWebServerRequest *request,
                                 const char *caller);

void delayedRestartTask(void *param);

static void safeCopy(char *dest, size_t destSize, const char *src);

static void shellyScanTask(void *parameter);

static void fillShellyJsonObj(const ALL_SHELLY_DEVICES &result, JsonArray &array);
static void fillShellyJsonObjWithErrorMsg(const ALL_SHELLY_DEVICES &result, JsonArray &array);

/* ---------- init ---------- */

void ajaxCalls_init(CALLBACK_GET_DATA getData, CALLBACK_SET_SETUP_CHANGED setupCh)
{
    if (!ajaxCalls_lock(g_ajaxMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
    {
        LOG_ERROR(TAG_AJAX, "ajaxCalls_init - mutex lock failed");
        return;
    }

    g_getDataCallback = getData;
    g_setSetupChangedCallback = setupCh;

    ajaxCalls_unlock(g_ajaxMutex);
}

// ajaxCalls_ensureInitPrimitives() entfallen – alle Mutexes werden zentral in appSyncInit() erstellt

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
/* ---------- helpers ---------- */static void safeCopy(char *dest, size_t destSize, const char *src)
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

static void shellyScanFinish(const String &json)
{
    safeCopy(g_shellyJsonCache, sizeof(g_shellyJsonCache), json.c_str());
    g_shellyScanRunning = false;
    g_shellyScanDone = true;
    g_shellyTaskHandle = nullptr;
}

static void fillShellyJsonObj(const ALL_SHELLY_DEVICES &result, JsonArray &array)
{
    if (result.shellyDevice == nullptr)
    {
        return;
    }

    JsonObject object1 = array.createNestedObject();
    object1["IP"] = result.shellyDevice->ip;
    object1["PORT"] = result.shellyDevice->port;
    object1["NAME"] = result.shellyDevice->name;
}

static void fillShellyJsonObjWithErrorMsg(const ALL_SHELLY_DEVICES &result, JsonArray &array)
{
    if (result.errorContainer == nullptr)
    {
        return;
    }

    JsonObject object1 = array.createNestedObject();
    object1["ERROR"] = result.errorContainer->errorMessage;
}

/* ---------- restart task ---------- */

/* ---------- shelly scan ---------- */

bool ajaxCalls_triggerShellyScan(void)
{
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
            shellyScanFinish(response);
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
            shellyScanFinish(response);
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
        shellyScanFinish(response);
        ajaxCalls_unlock(g_shellyMutex);
    }

    LOG_INFO(TAG_AJAX, "shellyScanTask - finished");
    vTaskDelete(nullptr);
}

void ajaxCalls_handleBuildAndGetShelly(AsyncWebServerRequest *request)
{
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

void ajaxCalls_handleGetFullSetup(AsyncWebServerRequest *request)
{
    Setup setup;
    eprom_getSetup(setup);
    JsonDocument data;
    JsonObject result = data["result"].to<JsonObject>();

    for (size_t i = 0; i < setupFieldsCount; i++)
    {
        setupFields[i]->serialize(setup, result);
    }
}

/* ---------- get setup ---------- */

void ajaxCalls_handleGetSetup(AsyncWebServerRequest *request)
{
    Setup setup;
    eprom_getSetup(setup);

    JsonDocument data;
    JsonObject result = data["result"].to<JsonObject>();

    LOG_INFO(TAG_AJAX, "ajaxCalls_handleGetSetup - begin");

    result[WLAN_ESSID] = setup.ssid;
    result[WLAN_PASSWD] = setup.passwd;
    result[IP_INVERTER] = setup.inverter;
    result[AMIS_READER_HOST] = setup.amisReaderHost;
    result[AMIS_READER_KEY] = setup.amisKey;

    result[FORCE_HEIZPATRONE] = setup.forceHeating;
    result[HEIZSTABLEISTUNG] = setup.heizstab_leistung_in_watt;
    result[TEMP_AUSSCHALTEN] = setup.tempMaxAllowedInGrad;
    result[TEMP_EINSCHALT] = setup.tempMinInGrad;

    result[LEGIONELLEN_DELTA_TIME] = setup.legionellenDelta;
    result[LEGIONELLEN_TEMP] = setup.legionellenMaxTemp;

    result[AKKU] = setup.akku;
    result[AKKU_PRIORI] = setup.akkuPriori;

    result[WWW_MQTT_HOST] = setup.mqttHost;
    result[WWW_MQTT_USER] = setup.mqttUser;
    result[WWW_MQTT_PASWWD] = setup.mqttPass;

    result[WWW_INFLUX_HOST] = setup.influxHost;
    result[WWW_INFLUX_TOKEN] = setup.influxToken;
    result[WWW_INFLUX_ORG] = setup.influxOrg;
    result[WWW_INFLUX_BUCKET] = setup.influxBucket;
    result[PID_EPSILON] = setup.epsilonML_PinManager;

    result[WWW_WATT_BIAS] = setup.wattSetupForTest;
    String response;
    unsigned httpCode = 200;

    if (data.overflowed())
    {
        LOG_ERROR(TAG_AJAX, "JsonDocument overflowed! Not enough memory.");
        data.clear(); // Alles raus, wir brauchen Platz für die Fehlermeldung
        data["result"] = nullptr;
        data["error"]["code"] = 0;
        data["error"]["msg"] = "JsonDocument ist zu groß!";
        httpCode = 500;
    }

    else
    {
        data["error"]["code"] = 1;
        data["error"]["msg"] = "";
    }

    serializeJson(data, response);
    LOG_INFO(TAG_AJAX, "Send AJAX Data %s", response.c_str());
    request->send(httpCode, "application/json", response);
}

/* ---------- store setup -----
 *********************************************************************
 */

template <typename Struct>
bool parseStruct(JsonObject obj,
                 Struct &s,
                 JsonDocument &res,
                 const FieldBase<Struct> *const *fields,
                 size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (!fields[i]->apply(obj, s, res))
            return false;
    }
    return true;
}

void ajaxCalls_handleStoreSetup(JsonDocument &json,
                                AsyncWebServerRequest *request,
                                bool isAPModus)
{
    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(nullptr);
    LOG_INFO(TAG_AJAX, "jaxCallsHandleStoreSetup::free stack words: %u", (unsigned)stackRemaining);
    JsonDocument result;

    auto docPtr = my_make_unique<JsonDocument>();
    *docPtr = json;
    JsonObject jsonObj = docPtr->as<JsonObject>();
    // JsonObject jsonObj = json.as<JsonObject>();

    CALLBACK_SET_SETUP_CHANGED cb = nullptr;

    if (!ajaxCalls_lock(g_ajaxMutex, pdMS_TO_TICKS(AJAX_MUTEX_TIMEOUT_MS)))
    {
        request->send(500, "application/json",
                      "{\"done\":0,\"error\":\"mutex lock failed\"}");
        return;
    }

    cb = g_setSetupChangedCallback;
    ajaxCalls_unlock(g_ajaxMutex);
    LOG_DEBUG(TAG_AJAX, "===========> cb: %d", cb == NULL);
    if (cb)
        cb(false); // for hotupdate, mal alles deaktivieren

    Setup setup;
    bool allSuccessful = true;

    LOG_DEBUG(TAG_AJAX, "===========> before parseStruct");

    for (size_t i = 0; i < setupFieldsCount; i++)
    {
        const char *key = setupFields[i]->getKey();
        if (key == NULL)
        {
            LOG_DEBUG(TAG_AJAX, "No Key available");
            jsonObj["error"] = "No Key available";
            allSuccessful = false;
        }
        else
        {
            if (!json[key].isNull())
            {
                // WICHTIG: setup ist jetzt auf dem Stack
                if (!setupFields[i]->update(setup, json[key]))
                {
                    allSuccessful = false;
                    /*char buf[50];
                    sprintf(buf, "Wrong type or null value for key: %s.", key);
                    strcpy(jsonObj["error"],buf);*/
                    LOG_DEBUG(TAG_AJAX, "Wrong type or null value for key: %s", key);
                    jsonObj["error"] = "Wrong type or null value for ";
                }
            }
        }
    }
    /*
     data["error"]["code"] = 0;
            data["error"]["msg"] = "JsonDocument ist zu groß!";
    */

    LOG_DEBUG(TAG_AJAX, "===========> after parseStruct");
    setup.phasen_leistung_in_watt = setup.heizstab_leistung_in_watt / 3;

    LOG_DEBUG(TAG_AJAX, "after parseStruct, ok: ");
    if (!allSuccessful)
    {
        returnFromStoreSetup(false, result, request, "caller 1");
        return;
    }

    LOG_DEBUG(TAG_AJAX, "Try to write Eprom in calling task.");

    eprom_storeSetup(setup);

    /*  if (!appTask_epromWriter(std::move(setup)))
     {
         result["error"] = "queue full";
         returnFromStoreSetup(false, result, request);
         vTaskDelay(pdMS_TO_TICKS(2000));
         return;
     } */
    returnFromStoreSetup(true, result, request, "caller 2");
    if (cb)
        cb(true);
    LOG_DEBUG(TAG_AJAX, "EXIT with isAppMod: %d", isAPModus);
    if (isAPModus)
    {
        static uint32_t delayMs = 10000U;
        LOG_DEBUG(TAG_AJAX, "ESP SHUTING DOWN FOR RESTARTING");
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

    LOG_DEBUG(TAG_AJAX, "delayedRestartTask - restart ing in several secs ");
    vTaskDelay(pdMS_TO_TICKS(delayMs));

    esp_restart();

    vTaskDelete(nullptr);
}

/* ---------- response helper ---------- */

static void returnFromStoreSetup(bool inputCorrect,
                                 JsonDocument &result,
                                 AsyncWebServerRequest *request,
                                 const char *caller)
{
    String response;
    unsigned httpCode = 200;
    if (inputCorrect)
    {
        result["done"] = 1;
        result["error"] = "";
        LOG_INFO(TAG_AJAX, "ReturnFromStoreSetup - no errors, caller: %s", caller);
    }
    else
    {
        result["done"] = 0;
        if (!result["error"].isNull())
        {
            result["error"] = "invalid input";
        }
        httpCode = 500;
        LOG_ERROR(TAG_AJAX, "ReturnFromStoreSetup(ERRORS) , caller: %s", caller);
    }
    LOG_INFO(TAG_AJAX, "Send AJAX Data %s", response.c_str());

    serializeJson(result, response);
    request->send(httpCode, "application/json", response);
}