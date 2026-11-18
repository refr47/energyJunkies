#include "gridLoads.h"

#ifdef FEATURE_MQTT
#include "app_state.h"
#include "curTime.h"
#include "mqtt.h"
#include "utils.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <time.h>

namespace {
// Replace these four IDs/names and nominal ratings for the installation.
// Runtime watts always come from MQTT; zero means no fixed rating is known.
GridLoadConfig loads[GRID_LOAD_COUNT] = {
    {"load1", "Verbraucher 1", 0, 1, GridLoadControl::Switch},
    {"load2", "Verbraucher 2", 0, 2, GridLoadControl::Switch},
    {"load3", "Verbraucher 3", 0, 3, GridLoadControl::Switch},
    {"load4", "Verbraucher 4", 0, 4, GridLoadControl::Switch},
};
char statusTopicStorage[GRID_LOAD_COUNT][64];
char commandTopics[GRID_LOAD_COUNT][64];
const char *statusTopics[GRID_LOAD_COUNT];
uint32_t nextCommandId = 1;
constexpr uint32_t STATUS_MAX_AGE_MS = 90000;
constexpr uint32_t COMMAND_RETRY_MS = 10000;
constexpr uint32_t RESTORE_HOLD_MS = 60000;
uint32_t lastShedAt = 0;

bool sendCommand(unsigned i, GridLoadRuntime &state, bool on, bool retry = false)
{
    if (!retry) state.commandId = ++nextCommandId;
    const time_t now = time(nullptr);
    char payload[96];
    snprintf(payload, sizeof(payload), "{\"on\":%s,\"commandId\":%u,\"validUntil\":%lld}",
             on ? "true" : "false", state.commandId, static_cast<long long>(now + 15));
    if (!mqtt_publish(commandTopics[i], payload)) return false;
    state.desiredOn = on;
    state.pending = true;
    state.lastCommandMs = millis();
    return true;
}

void onStatus(void *, const char *topic, const uint8_t *payload, unsigned length)
{
    unsigned i = 0;
    for (; i < GRID_LOAD_COUNT && strcmp(topic, statusTopics[i]); ++i) {}
    if (i == GRID_LOAD_COUNT || !time_isSynchronized() || !payload || length > 192) return;
    JsonDocument doc;
    if (deserializeJson(doc, payload, length) || !doc["on"].is<bool>() ||
        !doc["available"].is<bool>() || !doc["powerW"].is<uint32_t>() ||
        (!doc["ts"].is<int64_t>() && !doc["ts"].is<int>())) return;
    const int64_t now = time(nullptr);
    const int64_t stamp = doc["ts"].as<int64_t>();
    if (stamp < now - 90 || stamp > now + 10) return;
    const uint32_t watts = doc["powerW"].as<uint32_t>();
    if (watts > 1000000) return;
    if (!appLockData(50)) return;
    auto &s = g_app.webSockData.gridLoads.loads[i];
    s.on = doc["on"].as<bool>();
    s.available = doc["available"].as<bool>();
    s.actualW = s.on ? watts : 0;
    s.lastSeenMs = millis();
    s.fresh = true;
    const uint32_t ack = doc["commandId"] | 0U;
    if (s.pending && ack == s.commandId && s.on == s.desiredOn) {
        s.pending = false;
        if (!s.desiredOn) s.shedByUs = true;
        else { s.shedByUs = false; s.shedPowerW = 0; }
    }
    if (!s.pending && s.shedByUs && s.on) {
        s.shedByUs = false;
        s.shedPowerW = 0;
    }
    appUnlockData();
}

void onConnection(void *, bool connected, bool)
{
    if (connected || !appLockData(50)) return;
    for (auto &s : g_app.webSockData.gridLoads.loads) s.fresh = false;
    appUnlockData();
}
} // namespace

const GridLoadConfig *gridLoadConfigs() { return loads; }

bool gridLoadsInit()
{
    for (unsigned i = 0; i < GRID_LOAD_COUNT; ++i) {
        snprintf(statusTopicStorage[i], sizeof(statusTopicStorage[i]),
                 "energy/loads/%s/state", loads[i].id);
        snprintf(commandTopics[i], sizeof(commandTopics[i]),
                 "energy/loads/%s/set", loads[i].id);
        statusTopics[i] = statusTopicStorage[i];
    }
    return mqtt_register_load_receiver(statusTopics, GRID_LOAD_COUNT,
                                       loads, onStatus, onConnection);
}

bool gridLoadsCommandCurrent(const char *topic, const char *payload)
{
    unsigned i = 0;
    for (; i < GRID_LOAD_COUNT && strcmp(topic, commandTopics[i]); ++i) {}
    if (i == GRID_LOAD_COUNT || !time_isSynchronized()) return false;
    JsonDocument doc;
    if (deserializeJson(doc, payload) || !doc["on"].is<bool>() ||
        !doc["commandId"].is<uint32_t>() ||
        (!doc["validUntil"].is<int64_t>() && !doc["validUntil"].is<int>())) return false;
    if (doc["validUntil"].as<int64_t>() < time(nullptr)) return false;
    if (!appLockData(50)) return false;
    const auto &data = g_app.webSockData;
    const auto &s = data.gridLoads.loads[i];
    const bool on = doc["on"].as<bool>();
    const bool measured = !strcmp(data.gridDemand.status, "ready") ||
                          !strcmp(data.gridDemand.status, "collecting");
    const bool current = s.fresh && s.pending && s.commandId == doc["commandId"].as<uint32_t>() &&
        s.desiredOn == on && (on || (data.setupData.gridDemand.enabled &&
        data.setupData.gridDemand.enforceLimit && measured && data.energyState.valid &&
        !isEnergyNight(data.setupData.energyPolling)));
    appUnlockData();
    return current;
}

void gridLoadsEvaluate()
{
    auto &data = g_app.webSockData;
    const uint32_t now = millis();
    for (auto &s : data.gridLoads.loads)
        s.fresh = s.fresh && uint32_t(now - s.lastSeenMs) <= STATUS_MAX_AGE_MS;
    if (!data.setupData.mqttEnabled || !data.states.mqtt || !time_isSynchronized()) return;

    const auto &d = data.gridDemand;
    const bool night = !strcmp(d.status, "night");
    const bool disabled = !data.setupData.gridDemand.enabled || !data.setupData.gridDemand.enforceLimit;
    const bool measured = !strcmp(d.status, "ready") || !strcmp(d.status, "collecting");

    if (night || disabled) {
        for (unsigned i = 0; i < GRID_LOAD_COUNT; ++i) {
            auto &s = data.gridLoads.loads[i];
            if (!s.fresh) continue;
            if ((s.shedByUs || (s.pending && !s.desiredOn)) &&
                (!s.pending || !s.desiredOn)) sendCommand(i, s, true);
            else if (s.pending && s.desiredOn &&
                     uint32_t(now - s.lastCommandMs) >= COMMAND_RETRY_MS)
                sendCommand(i, s, true, true);
        }
        return;
    }
    if (!measured || !data.energyState.valid) return;

    for (unsigned i = 0; i < GRID_LOAD_COUNT; ++i) {
        auto &s = data.gridLoads.loads[i];
        if (s.pending && s.fresh && uint32_t(now - s.lastCommandMs) >= COMMAND_RETRY_MS)
            sendCommand(i, s, s.desiredOn, true);
    }

    int needed = d.externalReductionW;
    // The grid sample can predate a just-issued command. Do not shed another
    // load against that same old measurement while its response is pending.
    for (const auto &s : data.gridLoads.loads)
        if ((s.pending && !s.desiredOn) ||
            (s.shedByUs && int32_t(s.lastCommandMs - data.energyState.sampledAt) > 0))
            needed = std::max(0, needed - static_cast<int>(s.shedPowerW));
    if (needed > 0) {
        // Use the configured priority, independent of table position. Only
        // reported current watts count as possible shedding.
        bool selected[GRID_LOAD_COUNT] = {};
        for (unsigned n = 0; n < GRID_LOAD_COUNT && needed > 0; ++n) {
            int best = -1;
            for (unsigned i = 0; i < GRID_LOAD_COUNT; ++i) {
                const auto &s = data.gridLoads.loads[i];
                if (selected[i] || loads[i].control != GridLoadControl::Switch ||
                    !s.fresh || !s.available || !s.on || !s.actualW || s.pending) continue;
                if (best < 0 || loads[i].priority < loads[best].priority) best = i;
            }
            if (best < 0) break;
            selected[best] = true;
            auto &s = data.gridLoads.loads[best];
            const uint32_t power = s.actualW;
            if (sendCommand(best, s, false)) {
                s.shedPowerW = power;
                lastShedAt = now;
                needed -= static_cast<int>(power);
            }
        }
        return;
    }
    // Restore at most one load per hold period, in reverse priority order.
    if (uint32_t(now - lastShedAt) < RESTORE_HOLD_MS) return;
    int best = -1;
    for (unsigned i = 0; i < GRID_LOAD_COUNT; ++i) {
        const auto &s = data.gridLoads.loads[i];
        if (!s.fresh || !s.shedByUs || s.pending || !s.available) continue;
        if (best < 0 || loads[i].priority > loads[best].priority) best = i;
    }
    if (best >= 0) {
        auto &s = data.gridLoads.loads[best];
        const double headroom = d.allowedRemainingImportW - data.energyState.p_grid;
        if (headroom > s.shedPowerW + data.setupData.gridDemand.limitW * 0.05 + 500 &&
            sendCommand(best, s, true)) lastShedAt = now;
    }
}

#else
const GridLoadConfig *gridLoadConfigs() { return nullptr; }
bool gridLoadsInit() { return false; }
void gridLoadsEvaluate() {}
bool gridLoadsCommandCurrent(const char *, const char *) { return false; }
#endif
