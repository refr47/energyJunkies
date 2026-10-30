#include "thermalController.h"
#include "weather.h"
#include <algorithm>

bool ThermalController::mustStopHeating(int sensor1, int sensor2)
{
    return (sensor1 < 0 && sensor2 < 0) ||
           sensor1 >= WEATHER_PREHEAT_MAX_SAFE || sensor2 >= WEATHER_PREHEAT_MAX_SAFE;
}

int ThermalController::resolveInputTemp(int sensor1, int sensor2, int, int)
{
    if (sensor1 < 0 && sensor2 < 0) return -1;
    if (sensor1 < 0) return sensor2;
    if (sensor2 < 0) return sensor1;
    return (sensor1 + sensor2) / 2;
}

void ThermalController::evaluateMode(
    int temp, int effectiveMax, int tempMin,
    int legionellenMaxTemp, unsigned long legionellenDelta,
    unsigned long nowMS, bool &legionellaCycleActive,
    unsigned long &lastLegionellaMs, ControlMode &mode, bool &doML)
{
    mode = MODE_OFF;
    doML = false;
    if (temp < 0 || temp >= WEATHER_PREHEAT_MAX_SAFE) {
        legionellaCycleActive = false;
        return;
    }
    const int legioTarget = std::min(legionellenMaxTemp, WEATHER_PREHEAT_MAX_SAFE);
    const bool legioEnabled = legioTarget > 0 && legionellenDelta > 0;
    if (!legioEnabled) legionellaCycleActive = false;
    if (legionellaCycleActive) {
        if (temp >= legioTarget) {
            legionellaCycleActive = false;
            lastLegionellaMs = nowMS;
            return; // Target reached: turn off, never continue full-power heating.
        }
        mode = MODE_LEGIONELLA;
        return;
    }
    const uint64_t period = static_cast<uint64_t>(legionellenDelta) * 3600000ULL;
    if (legioEnabled && static_cast<uint32_t>(nowMS - lastLegionellaMs) >= period) {
        lastLegionellaMs = nowMS;
        if (temp < legioTarget) {
            legionellaCycleActive = true;
            mode = MODE_LEGIONELLA;
            return;
        }
    }
    effectiveMax = std::min(effectiveMax, WEATHER_PREHEAT_MAX_SAFE);
    if (temp >= effectiveMax) return;
    if (temp < tempMin) {
        mode = MODE_MIN_TEMP;
        return;
    }
    mode = MODE_AUTO;
    doML = true;
}

bool ThermalController::isTempAboveMax(int temp, int effectiveMax)
{
    return temp < 0 || temp >= std::min(effectiveMax, WEATHER_PREHEAT_MAX_SAFE);
}
bool ThermalController::isTempBelowMin(int temp, int tempMin)
{
    return temp >= 0 && temp < tempMin;
}
