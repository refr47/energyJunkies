#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>
#include "app_tasks.h"
#include "app_state.h"

#include "eprom.h"
#include "utils.h"
#include "shelly.h"
#include "templ.h"
#include "ajaxConst.h"
#include "setupFields.h"
#include "setupValidation.h"
#include "hotUpdate.h"
#include "mqtt.h"
#include "influx.h"
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
                                 const char *caller, unsigned errorCode = 400);

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
        LOG_ERROR(TAG_AJAX, "shellyScanTask - shelly_listAllDevices returned false");
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
    ajaxCalls_handleGetSetup(request);
}

void ajaxCalls_handleGetSetup(AsyncWebServerRequest *request)
{
    Setup setup;
    uint32_t revision = 0;
    const bool stored = eprom_getSetup(setup, &revision);
    JsonDocument data;
    JsonObject result = data["result"].to<JsonObject>();
    for (size_t i = 0; i < setupFieldsCount; ++i) setupFields[i]->serialize(setup, result);
    data["schema"] = 2;
    data["source"] = stored ? "stored" : "defaults";
    data["state"]["savedRevision"] = revision;
    data["state"]["appliedRevision"] = setupAppliedRevision();
    bool pending = revision != setupAppliedRevision();
#ifdef FEATURE_MQTT
    data["capabilities"]["mqtt"] = true;
    data["state"]["mqttRevision"] = mqtt_appliedRevision();
    pending = pending || mqtt_appliedRevision() != revision;
#else
    data["capabilities"]["mqtt"] = false;
#endif
#ifdef FEATURE_INFLUX
    data["capabilities"]["influx"] = true;
    data["state"]["influxRevision"] = influx_appliedRevision();
    pending = pending || influx_appliedRevision() != revision;
#else
    data["capabilities"]["influx"] = false;
#endif
    data["state"]["pending"] = pending;
    if (appLockData(50)) {
        data["state"]["mqttConnected"] = g_app.webSockData.states.mqtt;
        data["state"]["influxConnected"] = g_app.webSockData.states.influx;
        appUnlockData();
    }
    data["error"]["code"] = 1;
    data["error"]["msg"] = "";
    if (!stored) data["warning"] = "Keine gueltige gespeicherte Konfiguration; sichere Standardwerte geladen";
    if (data.overflowed()) {
        request->send(500, "application/json", "{\"error\":{\"code\":0,\"msg\":\"Nicht genug Speicher\"}}");
        return;
    }
    String response;
    serializeJson(data, response);
    request->send(200, "application/json", response);
}

/* ---------- store setup -----
 *********************************************************************
 */

void ajaxCalls_handleStoreSetup(JsonDocument &json,
                                AsyncWebServerRequest *request,
                                bool isAPModus)
{
    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(nullptr);
    LOG_INFO(TAG_AJAX, "jaxCallsHandleStoreSetup::free stack words: %u", (unsigned)stackRemaining);
    JsonDocument result;

    if (!json.is<JsonObject>())
    {
        result["error"] = "JSON object required";
        returnFromStoreSetup(false, result, request, "invalid root");
        return;
    }

    Setup setup;
    if (!eprom_getSetup(setup) && !isAPModus)
    {
        result["error"] = "Gespeicherte Konfiguration konnte nicht gelesen werden; keine Aenderung gespeichert";
        returnFromStoreSetup(false, result, request, "NVS read", 500);
        return;
    }
    JsonArray errors = result["errors"].to<JsonArray>();
    const bool valid = applySetupPatch(setup, json.as<JsonObject>(), errors);
    if (result.overflowed())
    {
        request->send(500, "application/json", "{\"done\":0,\"error\":\"Nicht genug Speicher fuer Validierung\"}");
        return;
    }
    if (!valid)
    {
        result["error"] = "Konfiguration ungueltig; keine Aenderung gespeichert";
        returnFromStoreSetup(false, result, request, "validation");
        return;
    }
    LOG_DEBUG(TAG_AJAX, "Try to write Eprom in calling task.");

    if (!appLockConfig(100)) {
        result["error"] = "Konfiguration beschaeftigt; bitte erneut versuchen";
        returnFromStoreSetup(false, result, request, "config busy", 503);
        return;
    }
    const bool restart = isAPModus || setupNeedsRestart(g_app.webSockData.setupData, setup);
    appUnlockConfig();
    uint32_t revision;
    if (!eprom_storeSetup(setup, &revision))
    {
        result["error"] = "failed to persist setup";
        returnFromStoreSetup(false, result, request, "NVS write", 500);
        return;
    }
#ifndef FEATURE_MQTT
    if (setup.mqttEnabled) result["warnings"].add("MQTT ist in dieser Firmware nicht aktiviert; Einstellungen wurden gespeichert.");
#endif
#ifndef FEATURE_INFLUX
    if (setup.influxEnabled) result["warnings"].add("Influx ist in dieser Firmware nicht aktiviert; Einstellungen wurden gespeichert.");
#endif
    result["revision"] = revision;
    result["apply"] = restart ? "restart_required" : "pending";
    returnFromStoreSetup(true, result, request, "stored");
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
    const uint32_t delayMs = *static_cast<const uint32_t *>(param);

    LOG_DEBUG(TAG_AJAX, "delayedRestartTask - restart ing in several secs ");
    vTaskDelay(pdMS_TO_TICKS(delayMs));

    esp_restart();

    vTaskDelete(nullptr);
}

/* ---------- response helper ---------- */

static void returnFromStoreSetup(bool inputCorrect,
                                 JsonDocument &result,
                                 AsyncWebServerRequest *request,
                                 const char *caller, unsigned errorCode)
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
        if (result["error"].isNull())
        {
            result["error"] = "invalid input";
        }
        httpCode = errorCode;
        LOG_ERROR(TAG_AJAX, "ReturnFromStoreSetup(ERRORS) , caller: %s", caller);
    }
    serializeJson(result, response);
    LOG_DEBUG(TAG_AJAX, "Send AJAX response, status: %u", httpCode);
    request->send(httpCode, "application/json", response);
}

/* ---------- clear logs ----------
 *
 * Resetzt den ESP32-LogRingBuffer thread-sicher.
 * Client ruft POST /clearLogs auf und erwartet {"done":1}.
 */

void ajaxCalls_handleClearLogs(AsyncWebServerRequest *request)
{
    RingBuffer &rb = g_app.webSockData.logBuffer;

    // RingBuffer hat eigenen Mutex → davon locken, nicht g_appMutex
    if (!ajaxCalls_lock(rb.mutex, pdMS_TO_TICKS(500)))
    {
        request->send(500, "application/json",
                      "{\"done\":0, \"error\":\"mutex lock failed\"}");
        return;
    }

    // Buffer leeren: Read = Write = 0, Flags zurücksetzen
    rb.readIndex = 0;
    rb.writeIndex = 0;
    rb.firstEntryMade = false;
    rb.lastAddedTime = 0;
    memset(&rb.lastAddedEntry, 0, sizeof(LogEntry));
    rb.active = true; // Buffer bleibt aktiv — neue Entries ok

    ajaxCalls_unlock(rb.mutex);

    LOG_INFO(TAG_AJAX, "ajaxCalls_handleClearLogs — ring buffer cleared");
    request->send(200, "application/json", "{\"done\":1}");
}