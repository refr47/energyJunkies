// =============================================================================
// mlPredictor.cpp — MlPredictor Implementation for OhmPilot
//
// Responsibilities:
//   - TinyNN prediction and training
//   - Reward computation for RL training
//   - Downward ramp limiting for cloud-cover smoothing
// =============================================================================

#include "mlPredictor.h"

// ──────────────────────────────────────────────────────────────────────────
// Constructors
// ──────────────────────────────────────────────────────────────────────────
MlPredictor::MlPredictor()
    : m_tempMin(45.0f), m_tempMax(60.0f), m_maxPower(1728)
{
}

// ──────────────────────────────────────────────────────────────────────────
// init — set configuration bounds
// ──────────────────────────────────────────────────────────────────────────
void MlPredictor::init(float tempMin, float tempMax, int maxPower)
{
    m_tempMin = tempMin;
    m_tempMax = tempMax;
    m_maxPower = maxPower;
}


#ifdef TINYNN_ENABLE
void MlPredictor::setTinyNN(TinyNN *nn)
{
    m_tinyNN = nn;
}
#endif
// ──────────────────────────────────────────────────────────────────────────
// computeReward — reward signal for TinyNN training
//
// Positive reward: high surplus utilization, temp increasing toward setpoint
// Negative reward: wasting surplus (grid feed-in), temp overshooting
// ──────────────────────────────────────────────────────────────────────────
#ifdef TINYNN_ENABLE
float MlPredictor::computeReward(float measuredPower, float targetPower, float currentTemp) const
{
    float reward = 0.0f;

    // Reward: match target to available surplus (efficient utilization)
    float utilization = targetPower / std::max(measuredPower, 1.0f);
    utilization = std::min(utilization, 1.0f);
    reward += utilization * 0.5f;

    // Reward: temperature approaching optimal mid-range
    int midTemp = static_cast<int>(m_tempMax + m_tempMin) / 2;
    int tempDiff = std::abs(static_cast<int>(currentTemp) - midTemp);
    int maxDiff = std::max(static_cast<int>(m_tempMax - m_tempMin), 1);
    float tempScore = 1.0f - (static_cast<float>(tempDiff) / static_cast<float>(maxDiff));
    reward += tempScore * 0.3f;

    // Bonus: if surplus is being fully consumed
    if (measuredPower > 0 && targetPower >= measuredPower * 0.9f)
    {
        reward += 0.2f;
    }

    return reward;
}
#endif

// ──────────────────────────────────────────────────────────────────────────
// adjustTarget — TinyNN preheat/boost adjustment
//
// No-op when TinyNN disabled, tinyNN is nullptr, or doML is false.
// Returns: adjusted targetPower (may add preheat boost).
// Trains network with experience replay. Updates output state members.
// ──────────────────────────────────────────────────────────────────────────
int MlPredictor::adjustTarget(int targetPower, int measuredPower, bool doML,
                              int currentSensor,
                              float weatherPvRatio, float weatherCloudAvg, float weatherOutTemp,
                              int weatherBonus, int onePhase,
                              int &outTINYNN_preheat, int &outTINYNN_buffer)
{
#ifndef TINYNN_ENABLE
    return targetPower;
#endif

    outTINYNN_preheat = 0;
    outTINYNN_buffer = 0;

#ifdef TINYNN_ENABLE
    if (m_tinyNN == nullptr || !doML)
        return targetPower;

    // ── Normalize temperature ──
    float normTemp = ((float)currentSensor - m_tempMin) / (m_tempMax - m_tempMin);
    normTemp = std::fmaxf(0.0f, std::fminf(1.0f, normTemp));

    // ── Predict ──
    TinyNNPrediction pred = m_tinyNN->predict(
        normTemp, weatherPvRatio, weatherCloudAvg, weatherOutTemp);

    outTINYNN_preheat = pred.preheat_score;
    outTINYNN_buffer = pred.buffer_pct;

    // ── Preheat boost ──
    if (pred.preheat_score > 0.5f && weatherBonus > 0)
    {
        int boost = static_cast<int>(onePhase * pred.preheat_score * pred.buffer_pct);
        targetPower += boost;
        targetPower = std::min(targetPower, m_maxPower);
    }

    // ── Remember experience ──
    (void)computeReward(
        static_cast<float>(measuredPower),
        static_cast<float>(targetPower),
        static_cast<float>(currentSensor));
    TinyNNExperience exp;
    exp.temp           = normTemp * 2.0f - 1.0f;
    exp.pv_ratio       = weatherPvRatio * 2.0f - 1.0f;
    exp.cloud_avg      = weatherCloudAvg;
    exp.temp_outside   = weatherOutTemp;
    exp.target_preheat = pred.preheat_score;
    exp.target_buffer  = pred.buffer_pct;
    m_tinyNN->remember(exp);
    m_tinyNN->trainReplay();
#endif

    return targetPower;
}

// ──────────────────────────────────────────────────────────────────────────
// clampRampDown — limit downward step for cloud-cover smoothing
//
// When PV surplus drops sharply (clouds passing), limit the rate of reduction
// so relays don't toggle instantly.
// Pure helper → static.
// ──────────────────────────────────────────────────────────────────────────
int MlPredictor::clampRampDown(float targetPower, float lastSmoothedPower)
{
    // Ramp-down limits (configurable constants)
    constexpr float MAX_DROP_TOTAL = 300.0f;
    constexpr float HYSTERESIS = 12.0f;

    float diff = targetPower - lastSmoothedPower;
    if (diff < 0)
    {
        float maxDrop = MAX_DROP_TOTAL + HYSTERESIS;
        if (std::abs(diff) > maxDrop)
            targetPower = lastSmoothedPower - maxDrop;
    }
    return std::max(0, static_cast<int>(targetPower));
}
