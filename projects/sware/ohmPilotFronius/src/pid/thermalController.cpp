#include "thermalController.h"

// ──────────────────────────────────────────────────────────────────────────
// resolveInputTemp — dual-sensor temperature resolution
//
// Both sensors negative  → return tempMaxAllowed (safe: above max → heater off)
// One sensor negative   → return other sensor
// Both valid            → return average
// ──────────────────────────────────────────────────────────────────────────
int ThermalController::resolveInputTemp(int sensor1, int sensor2,
                                        int tempMaxAllowed, int tempMin)
{
    if (sensor1 < 0 && sensor2 < 0)
        return tempMaxAllowed; // safe: above max → heater off
    if (sensor1 < 0)
        return sensor2;
    if (sensor2 < 0)
        return sensor1;
    return (sensor1 + sensor2) / 2;
}

// ──────────────────────────────────────────────────────────────────────────
// evaluateMode — control mode evaluation
//
// Determines the correct heating control mode based on:
//   1. Legionella cycle state (periodic high-temp sanitation)
//   2. Max temperature threshold (heater off to prevent overheat)
//   3. Min temperature threshold (frost protection → full power)
//   4. Normal auto mode (standard PID control)
// ──────────────────────────────────────────────────────────────────────────
void ThermalController::evaluateMode(
    int temp, int effectiveMax, int tempMin,
    int legionellenMaxTemp, unsigned long legionellenDelta,
    unsigned long nowMS,
    bool &legionellaCycleActive, unsigned long &lastLegionellaMs,
    ControlMode &mode, bool &doML)
{
    doML = false;
    mode = MODE_AUTO;

    // ── Legionella cycle handling ──
    if ((legionellenMaxTemp > 0) && (legionellenDelta > 0))
    {
        if (legionellaCycleActive)
        {
            // Already in legio mode — check if temp reached target
            if (temp >= legionellenMaxTemp)
            {
                mode = MODE_LEGIONELLA;
                doML = false; // No ML during legio
                LOG_INFO(TAG_THERMAL, "LEGIO (active, temp reached: %d >= %d)", temp, legionellenMaxTemp);
                return;
            }
        }
        else
        {
            // Check if enough time passed for next legio cycle
            unsigned long legioDeltaMs = legionellenDelta * 3600000UL; // hours→ms
            if (nowMS - lastLegionellaMs >= legioDeltaMs)
            {
                mode = MODE_LEGIONELLA;
                lastLegionellaMs = nowMS;
                doML = false;
                LOG_INFO(TAG_THERMAL, "LEGIO (cycle start after %lu ms)", nowMS - lastLegionellaMs);
                return;
            }
        }
    }

    // ── Max temp: heater off (overheat protection) ──
    if (temp >= effectiveMax)
    {
        mode = MODE_OFF;
        legionellaCycleActive = false; // cancel legio if max temp hit
        LOG_INFO(TAG_THERMAL, "OFF (temp %d >= effMax %d)", temp, effectiveMax);
        return;
    }

    // ── Min temp: full power (frost protection) ──
    if (temp < tempMin)
    {
        mode = MODE_MIN_TEMP;
        LOG_INFO(TAG_THERMAL, "FROST (temp %d < min %d)", temp, tempMin);
        return;
    }

    // ── Normal auto mode ──
    mode = MODE_AUTO;
    doML = true;
    LOG_DEBUG(TAG_THERMAL, "AUTO (temp %d in range [%d..%d) doML=1)", temp, tempMin, effectiveMax);
}

// ──────────────────────────────────────────────────────────────────────────
// isTempAboveMax — safety check
// ──────────────────────────────────────────────────────────────────────────
bool ThermalController::isTempAboveMax(int temp, int effectiveMax)
{
    return temp >= effectiveMax;
}

// ──────────────────────────────────────────────────────────────────────────
// isTempBelowMin — frost protection check
// ──────────────────────────────────────────────────────────────────────────
bool ThermalController::isTempBelowMin(int temp, int tempMin)
{
    return temp < tempMin;
}
