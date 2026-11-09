#pragma once

#include <algorithm>
#include <cmath>
#include <stdint.h>
#include <time.h>

struct HeatingPlanConfig {
    bool enabled = false;
    uint16_t startMinute = 21 * 60;
    uint16_t endMinute = 6 * 60;
    uint16_t emergencyC = 2;
    uint16_t boilerLiters = 200;
};

struct HeatingPlanState {
    const char *status = "disabled";
    bool windowActive = false;
    bool emergency = false;
    bool requestHeat = false;
    bool pollingOverride = false;
    int64_t windowEnd = 0;
    double requiredWh = 0;
    uint32_t estimatedSeconds = 0;
    int64_t latestStart = 0;
    int loadHeadroomW = -1;
};

inline bool heatingWindow(const HeatingPlanConfig &c, time_t now, time_t *endAt = nullptr) {
    if (!c.enabled || now < 1704067200 || c.startMinute == c.endMinute) return false;
    struct tm local;
    if (!localtime_r(&now, &local)) return false;
    const unsigned minute = local.tm_hour * 60U + local.tm_min;
    const bool active = c.startMinute < c.endMinute
        ? minute >= c.startMinute && minute < c.endMinute
        : minute >= c.startMinute || minute < c.endMinute;
    if (active && endAt) {
        struct tm end = local;
        end.tm_hour = c.endMinute / 60;
        end.tm_min = c.endMinute % 60;
        end.tm_sec = 0;
        if (c.startMinute > c.endMinute && minute >= c.startMinute) ++end.tm_mday;
        end.tm_isdst = -1;
        *endAt = mktime(&end);
    }
    return active;
}

inline bool heatingPlanNeedsPolling(const HeatingPlanConfig &c, time_t now,
    int tempC, bool demandEnforced, int immediateMinC = -1) {
    const int safetyMin = c.enabled ? c.emergencyC : immediateMinC;
    return heatingWindow(c, now) || (demandEnforced &&
        tempC >= 0 && tempC < safetyMin);
}

inline HeatingPlanState evaluateHeatingPlan(const HeatingPlanConfig &c, time_t now,
    int tempC, int targetC, int heaterW, int availableW, bool measurementValid) {
    HeatingPlanState s;
    if (!c.enabled) return s;
    s.emergency = tempC >= 0 && tempC < c.emergencyC;
    if (tempC < 0) { s.status = "sensorInvalid"; return s; }
    if (now < 1704067200) {
        s.status = s.emergency ? "emergency" : "timeInvalid";
        s.requestHeat = s.emergency;
        return s;
    }
    time_t end = 0;
    s.windowActive = heatingWindow(c, now, &end);
    s.windowEnd = s.windowActive ? end : 0;
    s.requiredWh = std::max(0, targetC - tempC) * double(c.boilerLiters) * 1.163;
    const int usableW = std::max(1, heaterW);
    s.estimatedSeconds = static_cast<uint32_t>(std::ceil(s.requiredWh * 3600.0 / usableW * 1.2));
    s.latestStart = s.windowActive ? std::max<int64_t>(now, int64_t(end) - s.estimatedSeconds) : 0;
    if (s.emergency) { s.status = "emergency"; s.requestHeat = true; return s; }
    if (!s.windowActive) { s.status = "outsideWindow"; return s; }
    if (tempC >= targetC) { s.status = "atTarget"; return s; }
    const bool deadline = now >= s.latestStart;
    const bool highLoad = measurementValid && availableW < heaterW;
    s.requestHeat = deadline || !highLoad;
    s.status = s.requestHeat ? (deadline ? "deadline" : "heating") : "waitingLoad";
    return s;
}
