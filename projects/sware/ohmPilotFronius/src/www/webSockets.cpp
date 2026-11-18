// ============================================================================
// webSockets.cpp — Thread-Safe WebSocket Server für ESP32 (FreeRTOS)
//
// Architektur-Notizen:
// - notifyClients() wird aus taskWeb und dem WS-Event-Task aufgerufen
// - g_app.webSockData wird parallel von taskTemperature, taskEnergy, taskPID beschrieben
// - jsonMutex schützt die Paketgenerierung und den Log-Arbeitsbuffer
// - appLockData() schützt das Lesen aus g_app.webSockData (DATA domain)
// ============================================================================

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "webSockets.h"
#include "app_sync.h"          // g_jsonMutex / g_jsonResultMutex
#include "utils.h"
#include "app_state.h"        // appLockData / appUnlockData
#include "energySource.h"     // g_activeSource
#ifdef FEATURE_MQTT
#include "mqttActivity.h"
#endif
#include <ArduinoJson.h>
#include <atomic>

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
#define JSON_OBJECT_BUFFER_LEN 8192

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
#ifdef FEATURE_MQTT
static std::atomic<bool> s_mqttSnapshotNeeded{true};
static uint32_t s_lastMqttSequence = 0;
static unsigned s_lastMqttCount = 0;
#endif

// Callback — nur in Init-Phase gesetzt, danach nur gelesen.
// Zugriff über lokale Variable zum Schutz gegen Write-races.
static WEBSOCK_DATA &(*s_webSockData)(void) = nullptr;

// g_jsonMutex wird zentral in appSyncInit() erstellt.
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

static void buildDemandInterval(JsonObject obj, const GridDemandInterval &value, bool closed)
{
    obj["start"] = value.start;
    obj["end"] = value.start ? value.start + 900 : 0;
    obj["complete"] = value.start && value.complete;
    obj["observedSeconds"] = value.observedSeconds;
    obj["importEnergyWh"] = nullptr;
    obj["averageImportW"] = nullptr;
    if (value.observedSeconds > 0) {
        obj["importEnergyWh"] = value.importEnergyWh;
        // Partial energy remains visible, but is never presented as a full average.
        if (value.complete) obj["averageImportW"] = value.importEnergyWh * 3600.0 / (closed ? 900.0 : value.observedSeconds);
    }
}

static void buildGridDemand(JsonObject live, const GridDemandState &s)
{
    JsonObject obj = live["gridDemand"].to<JsonObject>();
    obj["enabled"] = s.enabled;
    obj["enforceLimit"] = s.enforceLimit;
    obj["limitW"] = s.limitW;
    obj["projectionValid"] = s.projectionValid;
    obj["projectedAverageW"] = s.projectionValid ? s.projectedAverageW : 0;
    obj["externalReductionW"] = s.externalReductionW;
    obj["status"] = s.status;
    obj["measurementAgeMs"] = s.measurementAgeMs;
    obj["limiting"] = s.limiting;
    obj["maxHeaterPowerW"] = nullptr;
    if (s.maxHeaterPowerW >= 0) obj["maxHeaterPowerW"] = s.maxHeaterPowerW;
    JsonObject current = obj["current"].to<JsonObject>();
    buildDemandInterval(current, s.current, false);
    current["remainingEnergyWh"] = nullptr;
    current["allowedRemainingImportW"] = nullptr;
    if (s.enabled && s.current.start && !strcmp(s.status, "ready")) {
        current["remainingEnergyWh"] = s.remainingEnergyWh;
        current["allowedRemainingImportW"] = s.allowedRemainingImportW;
    }
    buildDemandInterval(obj["lastCompleted"].to<JsonObject>(), s.lastCompleted, true);
}

static void buildGridLoads(JsonObject live, const GridLoadsState &state)
{
    JsonArray list = live["externalLoads"].to<JsonArray>();
    const GridLoadConfig *config = gridLoadConfigs();
    if (!config) return;
    for (unsigned i = 0; i < GRID_LOAD_COUNT; ++i) {
        JsonObject row = list.add<JsonObject>();
        row["id"] = config[i].id;
        row["name"] = config[i].name;
        row["nominalW"] = config[i].nominalW;
        row["priority"] = config[i].priority;
        row["control"] = config[i].control == GridLoadControl::Switch ? "switch" : "adjustable";
        const auto &s = state.loads[i];
        row["fresh"] = s.fresh;
        row["on"] = s.on;
        row["available"] = s.available;
        row["actualW"] = s.fresh ? s.actualW : 0;
        row["pending"] = s.pending;
        row["shedByUs"] = s.shedByUs;
    }
}

static void buildHeatingPlan(JsonObject live, const HeatingPlanState &s)
{
    JsonObject obj = live["heatingPlan"].to<JsonObject>();
    obj["status"] = s.status;
    obj["windowActive"] = s.windowActive;
    obj["emergency"] = s.emergency;
    obj["requestHeat"] = s.requestHeat;
    obj["pollingOverride"] = s.pollingOverride;
    obj["windowEnd"] = s.windowEnd;
    obj["requiredWh"] = s.requiredWh;
    obj["estimatedSeconds"] = s.estimatedSeconds;
    obj["latestStart"] = s.latestStart;
    obj["loadHeadroomW"] = nullptr;
    if (s.loadHeadroomW >= 0) obj["loadHeadroomW"] = s.loadHeadroomW;
}

#ifdef FEATURE_MQTT
static void buildMqttEvent(JsonObject obj, const MqttActivityEvent &event, uint32_t now)
{
    obj["sequence"] = event.sequence;
    obj["direction"] = event.received ? "receive" : "publish";
    obj["topic"] = event.topic;
    obj["payload"] = event.payload;
    obj["ageMs"] = uint32_t(now - event.recordedAtMs);
}

static void buildMqttActivity(JsonObject live, const WEBSOCK_DATA &data,
                              const MqttActivitySnapshot &snapshot)
{
    JsonObject mqtt = live["mqtt"].to<JsonObject>();
    mqtt["enabled"] = data.setupData.mqttEnabled;
    mqtt["connected"] = data.states.mqtt;
    JsonArray events = mqtt["events"].to<JsonArray>();
    if (!data.setupData.mqttEnabled) return;

    const uint32_t now = millis();
    if (snapshot.hasPublished)
        buildMqttEvent(mqtt["lastPublished"].to<JsonObject>(), snapshot.lastPublished, now);
    if (snapshot.hasReceived)
        buildMqttEvent(mqtt["lastReceived"].to<JsonObject>(), snapshot.lastReceived, now);
    for (unsigned i = 0; i < snapshot.count; ++i)
        buildMqttEvent(events.add<JsonObject>(), snapshot.events[i], now);
}
#endif

static void buildLiveEnergyData(JsonObject &live, const WEBSOCK_DATA &data)
{
    // ── Alle Werte aus energyState (quelle-agnostisch) ──
#ifdef FEATURE_MQTT
    JsonObject mqtt = live["mqtt"].to<JsonObject>();
    mqtt["enabled"] = data.setupData.mqttEnabled;
    mqtt["connected"] = data.states.mqtt;
#endif
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

#ifdef FEATURE_MQTT
    if (!data.states.mqtt)
        bitMaster |= (1 << STATE_MQTT);
#endif

    if (data.setupData.forceHeating == 1)
        bitMaster |= (1 << STATE_FORCE_HEATING);
    if (data.states.wattBiasForTest)
        bitMaster |= (1 << STATE_WATT_BIAS);

    live[FEHLER] = bitMaster;
}

static bool serializePacket(JsonDocument &doc, String &packet, const char *name)
{
    const size_t bytes = measureJson(doc);
    if (doc.overflowed() || bytes >= JSON_OBJECT_BUFFER_LEN || !packet.reserve(bytes + 1)) {
        LOG_ERROR(TAG_WEB_SOCKETS, "%s JSON cannot be serialized (%u bytes)",
                  name, static_cast<unsigned>(bytes));
        return false;
    }
    serializeJson(doc, packet);
    LOG_DEBUG(TAG_WEB_SOCKETS, "%s JSON: %u bytes", name, static_cast<unsigned>(packet.length()));
    return true;
}

static AsyncWebSocket::SendStatus sendPacket(const char *name, const String &packet)
{
    const auto status = ws.textAll(packet);
    LOG_INFO(TAG_WEB_SOCKETS, "WebSocket %s: status=%d bytes=%u clients=%u heap=%u",
             name, static_cast<int>(status), static_cast<unsigned>(packet.length()),
             static_cast<unsigned>(ws.count()), static_cast<unsigned>(ESP.getFreeHeap()));
    return status;
}

// ──────────────────────────────────────────────────────────────────────────
// cleanupClients() entfernt beendete Verbindungen.
// ──────────────────────────────────────────────────────────────────────────

void cleanupClients()
{
    ws.cleanupClients();
}

// ──────────────────────────────────────────────────────────────────────────
// notifyClients() — getrennte Nachrichten halten jede WebSocket-Nachricht klein.
// Der JSON-Mutex schützt auch den statischen Arbeitsbuffer in utils_logRead().
// ──────────────────────────────────────────────────────────────────────────


void notifyClients()
{
    if (ws.count() == 0) return;

    WEBSOCK_DATA data;
    if (!appLockData(50)) {
        LOG_ERROR(TAG_WEB_SOCKETS, "Failed to acquire data lock for websock data");
        return;
    }
    if (!s_webSockData) {
        appUnlockData();
        return;
    }
    data = s_webSockData();
    appUnlockData();

    if (!xSemaphoreTake(g_jsonMutex, pdMS_TO_TICKS(100))) {
        LOG_ERROR(TAG_WEB_SOCKETS, "Failed to take JSON mutex");
        return;
    }

    String packet;
    {
        JsonDocument liveDoc;
        JsonObject live = liveDoc["live"].to<JsonObject>();
        buildLiveEnergyData(live, data);
        buildLiveDeviceData(live, data);
        if (!serializePacket(liveDoc, packet, "live")) {
            xSemaphoreGive(g_jsonMutex);
            return;
        }
    }
    if (sendPacket("live", packet) == AsyncWebSocket::DISCARDED) {
        xSemaphoreGive(g_jsonMutex);
        return;
    }

    packet = "";
    {
        JsonDocument detailDoc;
        JsonObject detail = detailDoc["live"].to<JsonObject>();
        buildGridDemand(detail, data.gridDemand);
        if (data.gridDemand.enabled) buildGridLoads(detail, data.gridLoads);
        buildHeatingPlan(detail, data.heatingPlan);
        if (serializePacket(detailDoc, packet, "detail")) sendPacket("detail", packet);
    }

#ifdef FEATURE_MQTT
    if (data.setupData.mqttEnabled) {
        MqttActivitySnapshot snapshot{};
        mqttActivity_snapshot(snapshot);
        const uint32_t sequence = snapshot.count ? snapshot.events[0].sequence : 0;
        const bool changed = sequence != s_lastMqttSequence || snapshot.count != s_lastMqttCount;
        const bool snapshotNeeded = s_mqttSnapshotNeeded.exchange(false);
        if (changed || snapshotNeeded) {
            JsonDocument mqttDoc;
            buildMqttActivity(mqttDoc["live"].to<JsonObject>(), data, snapshot);
            packet = "";
            if (serializePacket(mqttDoc, packet, "mqtt") &&
                sendPacket("mqtt", packet) != AsyncWebSocket::DISCARDED) {
                s_lastMqttSequence = sequence;
                s_lastMqttCount = snapshot.count;
            } else s_mqttSnapshotNeeded.store(true);
        }
    }
#endif

    if (ws.count() > 0) {
        JsonDocument logDoc;
        const int logCount = utils_logRead(g_app.webSockData.logBuffer, logDoc);
        if (logCount > 0) {
            packet = "";
            if (serializePacket(logDoc, packet, "log")) sendPacket("log", packet);
        }
    }

    xSemaphoreGive(g_jsonMutex);
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
#ifdef FEATURE_MQTT
        s_mqttSnapshotNeeded.store(true);
#endif
        LOG_INFO(TAG_WEB_SOCKETS,"WebSocket client #%u connected from %s (heap=%u)",
                 client->id(), client->remoteIP().toString().c_str(),
                 static_cast<unsigned>(ESP.getFreeHeap()));
        break;

    case WS_EVT_DISCONNECT:
        LOG_INFO(TAG_WEB_SOCKETS,"WebSocket client #%u disconnected (heap=%u)",
                 client->id(), static_cast<unsigned>(ESP.getFreeHeap()));
        break;

    case WS_EVT_DATA:
        // Daten verarbeiten — thread-safe durch notifyClients Mutex
        handleWebSocketMessage(arg, data, len);
        break;

    case WS_EVT_ERROR:
        LOG_WARNING(TAG_WEB_SOCKETS, "WebSocket client #%u close code=%u",
                    client->id(), arg ? *static_cast<uint16_t *>(arg) : 0);
        break;

    case WS_EVT_PING:
    case WS_EVT_PONG:
        break;
    }
}
