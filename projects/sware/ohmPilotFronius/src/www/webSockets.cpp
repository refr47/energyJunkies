// ============================================================================
// webSockets.cpp — Thread-Safe WebSocket Server für ESP32 (FreeRTOS)
//
// Architektur-Notizen:
// - getJsonObj() wird aus versch. FreeRTOS Tasksgerufen (taskWeb + WS-Event-Task)
// - g_app.webSockData wird parallel von taskTemperature, taskEnergy, taskPID beschrieben
// - jsonMutex schützt JsonDocument + static Buffer-Generierung
// - appLockData() schützt das Lesen aus g_app.webSockData (DATA domain)
// - jsonResultMutex schützt den Ergebniszeiger nach Serialisierung
// ============================================================================

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "webSockets.h"
#include "app_sync.h"          // g_jsonMutex / g_jsonResultMutex
#include "utils.h"
#include "app_state.h"        // appLockData / appUnlockData
#include "energySource.h"     // g_activeSource
#include <ArduinoJson.h>

// ──────────────────────────────────────────────────────────────────────────
// Defines
// ──────────────────────────────────────────────────────────────────────────

#define PRODUKTION "solEr"
#define EIGENVERBRAUCH "ev"
#define NETZ_BEZUG "netzBezug"
#define HEIZSTAB_LEISTUNG_PHASE "hsPhase"
#define STROM_EXPORT_INS "SEI"
#define STROM_IMPORT_INS "SII"

#define TEMP_PUFFERSPEICHER "bTemp"

#define HEIZPATRONE_L3 "L3"
#define HEIZPATRONE_L2 "L2"
#define HEIZPATRONE_L1 "L1"
#define FEHLER "errors"
#define AAKU_AVAILABLE "aakHasBattery"
#define AKKU_CAPACITA "aakPower"
#define AKKU_ZUSTAND "aakStat"
#define AKKU_ENTLADEN "aakEntladen"
#define BATTERY_CAPACITY_KW 14000 // fixe Batteriekapazitaet

#define NETZ_EXPORT_INS "NEI"
#define NETZ_IMPORT_INS "NII"
#define FORCE_HEIZPATRONE "forceHeizung"
#define FORCE_HEIZPATRONE_STOP "forceHeizungOff"
#define EPSILON_PIN_MANAGER "EpsilonPin"

#define STATE_CARDWRITE 0      // 1 << 0 = 1
#define STATE_FLASH 1          // 1 << 1 = 2
#define STATE_MODBUS 2         // 1 << 2 = 4
#define STATE_TEMPSENSOR 3     //      1 << 3 = 8
#define STATE_BOILER_HEATING 4 // 1 << 4 = 16
#define STATE_SOURCE_DOWN 5    // 1 << 5 = 32  (energy source unavailable)
#define STATE_MQTT 6           // 1 << 6 = 64
#define STATE_INFLUX 7         // 1 << 7 = 128
#define STATE_WATT_BIAS 8      // 1 << 8 = 256
#define STATE_FORCE_HEATING 9  // 1 << 9 = 512

#define FORMAT_BUFFER_LEN 35
#define JSON_OBJECT_BUFFER_LEN 2048

// ──────────────────────────────────────────────────────────────────────────
// Prototypes
// ──────────────────────────────────────────────────────────────────────────

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

// ──────────────────────────────────────────────────────────────────────────
// Statische Shared Resources — alle mit Mutex-geschütztem Zugriff
// ──────────────────────────────────────────────────────────────────────────

static AsyncWebSocket ws("/ws");

// Callback — nur in Init-Phase gesetzt, danach nur gelesen.
// Zugriff über lokale Variable zum Schutz gegen Write-races.
static WEBSOCK_DATA &(*s_webSockData)(void) = nullptr;

// jsonMutex: schuetzt JsonDocument serialisierung + static Buffer
// extern SemaphoreHandle_t g_jsonMutex;
// extern SemaphoreHandle_t g_jsonResultMutex;
// (zentral erstellt in appSyncInit() → siehe app_sync.h)
AsyncWebSocket *webSockets_init(CALLBACK_GET_DATA getData)
{
    // (Mutexes werden zentral in appSyncInit() erstellt → kein lokaler Aufruf mehr)

    // Callback-Pointer atomar setzen (nach Mutex-Init)
    s_webSockData = getData;

    ws.onEvent(onEvent);
    return &ws;
}

// ──────────────────────────────────────────────────────────────────────────
// JSON Builder Helpers (isolated per data domain)
// ──────────────────────────────────────────────────────────────────────────

static double s_prevValueFromSmartMeter = 0.0;

static void buildLiveEnergyData(JsonObject &live, const WEBSOCK_DATA &data)
{
    // ── Alle Werte aus energyState (quelle-agnostisch) ──
    auto &es = data.energyState;
    live["energyValid"] = es.valid;
    live["energySaving"] = es.saving;
    live["energyNight"] = es.night;
    live["energyAgeMs"] = es.sampleSequence ? uint32_t(millis() - es.sampledAt) : 0;
    live[PRODUKTION]         = es.p_pv;
    live[NETZ_BEZUG]         = es.p_grid;
    live[EIGENVERBRAUCH]     = es.p_load;
    live[AKKU_ZUSTAND]       = es.soc;
    live[AKKU_ENTLADEN]      = es.p_akku;
    if (data.setupData.akku)
    {
        live[AKKU_CAPACITA]  = BATTERY_CAPACITY_KW;
    }

    // dataSource-String
    EnergySource* source = g_activeSource.load();
    if (source)
    {
        live["dataSource"] = source->name();
    }
    else
    {
        live["dataSource"] = "none";
    }
    // else if (data.states.modbusOK)
    // {
    //     live[PRODUKTION] = data.energyState.p_pv;
    //
    //     if (data.energyState.p_pv
    //         + data.energyState.p_grid >= 0.0)
    //     {
    //         live[NETZ_BEZUG]   = data.energyState.p_grid;
    //         live[EIGENVERBRAUCH] = data.energyState.p_pv
    //                               + data.energyState.p_grid;
    //         s_prevValueFromSmartMeter = data.energyState.p_grid;
    //     }
    //     else
    //     {
    //         live[NETZ_BEZUG]    = s_prevValueFromSmartMeter;
    //         live[EIGENVERBRAUCH] = data.energyState.p_pv
    //                               + s_prevValueFromSmartMeter;
    //     }
    
    // energieState enthält alle relevanten Werte,
    // dataSource-String wird oben gesetzt
}

static void buildLiveDeviceData(JsonObject &live, const WEBSOCK_DATA &data)
{
    // Durchschnittstemperatur + Alarm-Formatierung
    char formatBuffer[FORMAT_BUFFER_LEN] = {0};
    int avgTemp = (data.temperature.sensor1 + data.temperature.sensor2) / 2;

    if (data.temperature.alarm)
    {
        snprintf(formatBuffer, FORMAT_BUFFER_LEN, "!! %d !! ", avgTemp);
    }
    else
    {
        snprintf(formatBuffer, FORMAT_BUFFER_LEN, "%d", avgTemp);
    }

    live[TEMP_PUFFERSPEICHER]     = formatBuffer;
    live[HEIZPATRONE_L1]         = data.pidContainer.PID_PIN1;
    live[HEIZPATRONE_L2]         = data.pidContainer.PID_PIN2;
    live[HEIZPATRONE_L3]         = data.pidContainer.mAnalogOut;
    live[FORCE_HEIZPATRONE]      = (int)data.setupData.forceHeating;
    live[FORCE_HEIZPATRONE_STOP] = (int)data.setupData.forceHeatingStop;
    // ── Aktuelles Phasen-Performance: uses pidContainer.mCurrentPower (actual) 
    //     divided by active phase count → per-phase power in W sent to frontend ──
    int activePhases = (data.pidContainer.PID_PIN1 ? 1 : 0) + (data.pidContainer.PID_PIN2 ? 1 : 0);
    int perPhaseWatt = (activePhases > 0) && data.pidContainer.mCurrentPower > 0
                        ? data.pidContainer.mCurrentPower / activePhases
                        : (int)(data.setupData.heizstab_leistung_in_watt / 3);
    live[HEIZSTAB_LEISTUNG_PHASE] = perPhaseWatt;

    // ── Wetter-Forecast ── (immer vorhanden, Dummy-Werte wenn WEATHER_API aus)
    live["weatherBonus"]       = data.pidContainer.weatherBonus;
    live["weatherPvRatio"]     = data.pidContainer.weatherPvRatio;
    live["weatherCloudAvg"]    = data.pidContainer.weatherCloudAvg;
    live["weatherOutTemp"]     = data.pidContainer.weatherOutTemp;
    // ── Weather prediction consideration status ──
    live["weatherPredictionUsed"] = data.pidContainer.weatherBonus > 0 ? 1 : 0;

#ifdef TINYNN_ENABLE
    // ── TinyNN-Neural-Network Prediction ──
    live["tinyNN_preheat"]     = data.pidContainer.tinyNN_preheat_score;
    live["tinyNN_buffer"]      = data.pidContainer.tinyNN_buffer_pct;
#endif

    // ── Early heating mode status ──
    live["earlyHeatingMode"] = (data.setupData.forceHeating == 2) ? 1 : 0;

    live[AAKU_AVAILABLE]         = data.setupData.akku;

    // ── Akkudaten kommen aus buildLiveEnergyData() (data-source-aware) ──

    // Error-Bitmask
    unsigned int bitMaster = 0;
    if (!data.states.flashOK)
        bitMaster |= (1 << STATE_FLASH);


    if (!data.states.tempSensorOK)
        bitMaster |= (1 << STATE_TEMPSENSOR);
    if (!data.states.boilerHeating)
        bitMaster |= (1 << STATE_BOILER_HEATING);

    // Energy-Source down (quelle-agnostisch)
    if (!data.energyState.valid)
        bitMaster |= (1 << STATE_SOURCE_DOWN);

#ifdef MQTT
    if (!data.states.mqtt)
        bitMaster |= (1 << STATE_MQTT);
#endif

    if (data.setupData.forceHeating == 1)
        bitMaster |= (1 << STATE_FORCE_HEATING);
    if (data.states.wattBiasForTest)
        bitMaster |= (1 << STATE_WATT_BIAS);

    live[FEHLER] = bitMaster;
}

// =============================================================================
// getJsonObj() — thread-safe JSON serialisierung für WebSocket-Response
//
// Design:
// 1. appLockData(): Lese g_app.webSockData konsistent
// 2. Lokale Kopie aller Daten → appUnlockData()
// 3. jsonMutex: Serialisiere Builder → static Buffer
// 4. Return const char*
// =============================================================================
static const char *getJsonObj()
{
    // ── Schritt 1: Daten konsistent aus g_app lesen ──────────────────────
    WEBSOCK_DATA data;

    if (!appLockData(50))
    {
        LOG_ERROR(TAG_WEB_SOCKETS, "Failed to acquire data lock for websock data");
        return "{}";
    }

    WEBSOCK_DATA &(*localGetData)(void) = s_webSockData;
    if (localGetData != nullptr)
    {
        data = localGetData();
    }
    appUnlockData();

    // ── Schritt 2: jsonMutex für Serialisierung ─────────────────────────
    static char jsonObjBuffer[JSON_OBJECT_BUFFER_LEN] = {0};

    if (!xSemaphoreTake(g_jsonMutex, pdMS_TO_TICKS(100)))
    {
        LOG_ERROR(TAG_WEB_SOCKETS, "Failed to take JSON mutex");
        return "{}";
    }

    JsonDocument doc;
    JsonObject live = doc.createNestedObject("live");

    // ── Schritt 3: Builder ──────────────────────────────────────────────
    buildLiveEnergyData(live, data);
    buildLiveDeviceData(live, data);

    // ── Schritt 4: Log-Buffer ───────────────────────────────────────────
    LOG_DEBUG(TAG_WEB_SOCKETS, "Preparing JSON log entries, log buffer active: %d", data.logBuffer.active);
    int count = utils_logRead(data.logBuffer, doc);
    LOG_DEBUG(TAG_WEB_SOCKETS, "Creating JSON log entries - count: %d", count);

    // ── Schritt 5: Serialisieren ────────────────────────────────────────
    size_t bytesWritten = serializeJson(doc, jsonObjBuffer);
    jsonObjBuffer[bytesWritten] = '\0';

    xSemaphoreGive(g_jsonMutex);
    LOG_DEBUG(TAG_WEB_SOCKETS, "JSON prepared, %zu bytes", bytesWritten);
    return jsonObjBuffer;
}

// ──────────────────────────────────────────────────────────────────────────
// cleanupClients() — waehrend der Cleanup-Zeit keine Clients, also
// kein paralleler data-Transfer moeglich
// ──────────────────────────────────────────────────────────────────────────

void cleanupClients()
{
    ws.cleanupClients();
}

// ──────────────────────────────────────────────────────────────────────────
// notifyClients() — JSON erstellen + an alle Clients senden
//
// Thread-Sicherheit:
// - getJsonObj() ist thread-safe (jsonMutex)
// - ws.textAll() bekommt einen String (Value), nicht einen Zeiger auf static
//   Buffer. Damit kann ein anderer Thread den Buffer zwischen Generierung
//   und Senden nicht zerstoeren.
// - ws.count() > 0 Check und textAll() sind atomar aus Sicht von AsyncWebSocket
// ──────────────────────────────────────────────────────────────────────────

void notifyClients()
{
    // String() kopiert den Inhalt — kein Dangling Pointer mehr!
    String jsonData = getJsonObj();

    // Nur senden wenn Clients verbunden — textAll internal ist thread-safe
    // im Kontext von ESPAsyncWebServer (Single-Owner-Client-List)
    if (ws.count() > 0)
    {
        ws.textAll(jsonData);
    }
    else
    {
        LOG_DEBUG(TAG_WEB_SOCKETS, "No clients connected, skipping notify");
    }
}

// ──────────────────────────────────────────────────────────────────────────
// handleWebSocketMessage() — vom AsyncWebSocket-Event-Thread angerufen
// ──────────────────────────────────────────────────────────────────────────

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;

    // Nur Text-Frames verarbeiten (Komplette Nachricht in einem Frame)
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        LOG_INFO(TAG_WEB_SOCKETS, "webSockets::handleWebSocketMessage: %.*s",
                 (int)len, (char *)data);

        // Antwort an ALLE Clients senden (broadcast) — thread-sicher durch
        // notifyClients() Implementation
        notifyClients();
    }
}

// ──────────────────────────────────────────────────────────────────────────
// onEvent() — Haupt-Event-Handler fuer WebSocket-Ereignisse
//
// Wird vom IDF Network Task (Core 0) asynchron aufgerufen.
// WS_EVT_DATA triggert notifyClients(), was thread-safe ist.
// ──────────────────────────────────────────────────────────────────────────

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n",
                      client->id(), client->remoteIP().toString().c_str());
        break;

    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;

    case WS_EVT_DATA:
        // Daten verarbeiten — thread-safe durch notifyClients Mutex
        handleWebSocketMessage(arg, data, len);
        break;

    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        // Ping/Pong und Fehler werden ignoriert (Library-Intern)
        break;
    }
}
