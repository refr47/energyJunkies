#pragma once
#include <Arduino.h>
#ifdef TINYNN_ENABLE
#include "TinyNN.h"  // TinyNN, TinyNNPrediction, TinyNNExperience
#endif

// =============================================================================
// MlPredictor — TinyNN prediction, training, and ML helpers
//
// Encapsulates:
//   - TinyNN prediction with preheat boost
//   - Experience replay and training
//   - Reward computation for RL training
//   - Downward ramp limiting for cloud-cover smoothing
//
// Universal compatibility:
//   - TINYNN_ENABLE disabled → all helpers return neutral values
//   - All public methods are safe to call even when tinyNN is nullptr
// =============================================================================
class MlPredictor
{
    friend class PinManager;

public:
    MlPredictor();

    // ── Initialization ────────────────────────────────────────────────────
    void init(float tempMin, float tempMax, int maxPower);

#ifdef TINYNN_ENABLE
    void setTinyNN(TinyNN *nn);
#endif

    // ── Core: Adjust target power with ML prediction ─────────────────────
    // Returns adjusted target power (may add preheat boost).
    // Uses tinyNN predict(), trainReplay(), experience replay.
    int adjustTarget(int targetPower, int measuredPower, bool doML,
                    int currentSensor,
                    float weatherPvRatio, float weatherCloudAvg, float weatherOutTemp,
                    int weatherBonus, int onePhase,
                    int &outTINYNN_preheat, int &outTINYNN_buffer);

    // ── Clamp down-ramp for cloud-cover smoothing ────────────────────────
    // Limits how fast target power can drop when PV surplus decreases
    // (prevents relay toggling on cloud cover)
    int clampRampDown(float targetPower, float lastSmoothedPower);

private:
#ifdef TINYNN_ENABLE
    TinyNN *m_tinyNN = nullptr;
#endif

#ifdef TINYNN_ENABLE
    float computeReward(float measuredPower, float targetPower, float currentTemp) const;
#endif

    // ── Configuration (once at startup) ──────────────────────────────────
    float m_tempMin = 45.0f;
    float m_tempMax = 60.0f;
    int   m_maxPower = 1728; // 3 phases × onePhase

    // ── Ramp-down limiter state ─────────────────────────────────────────
    float m_lastSmoothedPower = 0.0f;
};
