#include "mlPredictor.h"
#include "weather.h"
#include <algorithm>
#include <cmath>

int MlPredictor::temperatureBonus(int ruleBonus, bool forecastValid, int temperature,
                                 int normalMax, int tempMin, float pvRatio,
                                 float cloud, float outside, unsigned long now,
                                 float &preheat, float &buffer)
{
    preheat = buffer = 0;
    if (!forecastValid || temperature < 0 || !std::isfinite(pvRatio) ||
        !std::isfinite(cloud) || !std::isfinite(outside)) return 0;
    ruleBonus = std::max(0, std::min(ruleBonus, WEATHER_PREHEAT_MAX_BONUS));
#ifdef TINYNN_ENABLE
    if (!m_tinyNN) return ruleBonus;
    auto clamp = [](float x, float lo, float hi) { return std::max(lo, std::min(x, hi)); };
    // Exactly the same normalized inputs are used for inference and replay.
    TinyNNExperience sample{};
    const float range = std::max(1, WEATHER_PREHEAT_MAX_SAFE - tempMin);
    sample.temp = clamp(2.0f * (temperature - tempMin) / range - 1.0f, -1, 1);
    sample.pv_ratio = clamp(pvRatio / 2.0f - 1.0f, -1, 1);
    sample.cloud_avg = clamp(cloud, 0, 1);
    sample.temp_outside = clamp((outside - 10.0f) / 30.0f, -1, 1);
    // Supervised rule targets, not the network's own predictions. These are
    // planning heuristics, not a learned household hot-water demand model.
    sample.target_preheat = ruleBonus / static_cast<float>(WEATHER_PREHEAT_MAX_BONUS);
    const int target = std::min(WEATHER_PREHEAT_MAX_SAFE, normalMax + ruleBonus);
    sample.target_buffer = clamp((target - temperature) / range, 0, 1);
    const TinyNNPrediction prediction = m_tinyNN->predict(sample.temp, sample.pv_ratio,
                                                         sample.cloud_avg, sample.temp_outside);
    if (!std::isfinite(prediction.preheat_score) || !std::isfinite(prediction.buffer_pct))
        return ruleBonus;
    preheat = prediction.preheat_score;
    buffer = prediction.buffer_pct;
    // Do not replay essentially identical observations every two seconds.
    if (!m_haveTrainingSample || now - m_lastTraining >= 30UL * 60 * 1000) {
        m_haveTrainingSample = true;
        m_lastTraining = now;
        m_tinyNN->remember(sample);
        m_tinyNN->trainReplay();
    }
    // Cold-start fallback; ML cannot invent preheating or reduce a full reserve.
    if (ruleBonus == 0 || ruleBonus == WEATHER_PREHEAT_MAX_BONUS ||
        m_tinyNN->sampleCount() < 100) return ruleBonus;
    const float correction = (preheat - sample.target_preheat) +
                             (buffer - sample.target_buffer);
    return std::max(0, std::min(WEATHER_PREHEAT_MAX_BONUS,
        ruleBonus + static_cast<int>(std::lround(clamp(correction * 2, -2, 2)))));
#else
    (void)normalMax; (void)tempMin; (void)now;
    return ruleBonus;
#endif
}

int MlPredictor::clampRampDown(float targetPower, float lastSmoothedPower)
{
    if (targetPower < lastSmoothedPower - 312.0f)
        targetPower = lastSmoothedPower - 312.0f;
    return std::max(0, static_cast<int>(targetPower));
}
