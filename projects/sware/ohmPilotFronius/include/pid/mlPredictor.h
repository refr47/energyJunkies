#pragma once
#include <Arduino.h>
#ifdef TINYNN_ENABLE
#include "TinyNN.h"
#endif

class MlPredictor {
public:
#ifdef TINYNN_ENABLE
    void setTinyNN(TinyNN *nn) { m_tinyNN = nn; }
#endif
    // Learn a bounded temperature-reserve correction from explicit rule targets.
    // Never adjusts electrical power or overrides an absent/expired forecast.
    int temperatureBonus(int ruleBonus, bool forecastValid, int temperature,
                         int normalMax, int tempMin, float pvRatio,
                         float cloud, float outside, unsigned long now,
                         float &preheat, float &buffer);
    int clampRampDown(float targetPower, float lastSmoothedPower);
private:
#ifdef TINYNN_ENABLE
    TinyNN *m_tinyNN = nullptr;
#endif
    bool m_haveTrainingSample = false;
    unsigned long m_lastTraining = 0;
};
