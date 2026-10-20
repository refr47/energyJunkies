#pragma once
#include <Arduino.h>
#include "defines.h"        // LOG_* macros, TAG_*, enum ControlMode
#include "phaseController.h"

// =============================================================================
// ThermalController — Temperature resolution \& control mode evaluation
//
// Encapsulates:
//   - Dual-sensor temperature resolution (fallback on sensor failure)
//   - Control mode evaluation (Legionella, OFF, MIN_TEMP, AUTO)
//   - Legionella cycle state management
//
// Stateless helpers + mutable Legionella state (passed by ref)
// =============================================================================
class ThermalController
{
public:
    // ── Temperature resolution ───────────────────────────────────────────
    // Compute average from two sensors. Handles negative values (sensor fail).
    // Returns effective temperature clamped to tempMaxAllowed on dual-fail.
    static int resolveInputTemp(int sensor1, int sensor2,
                                int tempMaxAllowed, int tempMin);

    // ── Control mode evaluation ──────────────────────────────────────────
    // Determine control mode based on temperature + legionella cycle state.
    //
    // Returns: ControlMode (MODE_OFF, MODE_LEGIONELLA, MODE_MIN_TEMP, MODE_AUTO)
    // Side-effects: updates legionellaCycleActive \& lastLegionellaMs
    static void evaluateMode(
        int temp, int effectiveMax, int tempMin,
        int legionellenMaxTemp, unsigned long legionellenDelta,
        unsigned long nowMS,
        // Mutable legionella state (passed by ref)
        bool &legionellaCycleActive, unsigned long &lastLegionellaMs,
        // Output
        ControlMode &mode, bool &doML);

    // ── Safety checks ────────────────────────────────────────────────────
    // Returns true if temperature is above max (heater must be off)
    static bool isTempAboveMax(int temp, int effectiveMax);
    // Returns true if temperature is below min (frost protection, full power)
    static bool isTempBelowMin(int temp, int tempMin);
};
