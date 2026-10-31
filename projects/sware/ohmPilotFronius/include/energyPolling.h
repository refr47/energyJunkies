#pragma once
#include <stdint.h>
#include <cmath>

#ifndef ENERGY_DAY_INTERVAL
#define ENERGY_DAY_INTERVAL 40000
#endif
#ifndef ENERGY_NIGHT_INTERVAL
#define ENERGY_NIGHT_INTERVAL 300000
#endif
#ifndef ENERGY_NIGHT_OFF_START
#define ENERGY_NIGHT_OFF_START 23
#endif
#ifndef ENERGY_NIGHT_OFF_END
#define ENERGY_NIGHT_OFF_END 5
#endif
#ifndef ENERGY_LOW_PV_WATTS
#define ENERGY_LOW_PV_WATTS 20
#endif
#ifndef ENERGY_WAKE_PV_WATTS
#define ENERGY_WAKE_PV_WATTS 50
#endif
#ifndef ENERGY_LOW_PV_HOLD_MS
#define ENERGY_LOW_PV_HOLD_MS 300000
#endif

// Equal start/end disables the window; also supports windows within one day.
inline bool energyNightHour(int hour, int start = ENERGY_NIGHT_OFF_START,
                            int end = ENERGY_NIGHT_OFF_END)
{
    if (hour < 0 || hour > 23 || start == end) return false;
    return start < end ? hour >= start && hour < end : hour >= start || hour < end;
}

// Access under DATA lock. Only distinct, successful samples advance the PV rule.
class EnergyPollingPolicy {
    uint32_t lastSequence = 0;
    uint32_t lowSince = 0;
    bool trackingLow = false;
    bool saving = false;
public:
    bool isSaving() const { return saving; }
    uint32_t update(uint32_t now, bool night, bool valid, uint32_t sequence, double pv)
    {
        if (night || !valid || !std::isfinite(pv)) {
            trackingLow = false;
            saving = false;
            lastSequence = sequence;
            return night ? 0 : ENERGY_DAY_INTERVAL;
        }
        if (sequence != lastSequence) {
            lastSequence = sequence;
            if (pv > ENERGY_WAKE_PV_WATTS) {
                saving = false;
                trackingLow = false;
            } else if (!saving) {
                if (pv < ENERGY_LOW_PV_WATTS) {
                    if (!trackingLow) { lowSince = now; trackingLow = true; }
                    if (uint32_t(now - lowSince) >= ENERGY_LOW_PV_HOLD_MS) saving = true;
                } else trackingLow = false;
            }
        }
        return saving ? ENERGY_NIGHT_INTERVAL : ENERGY_DAY_INTERVAL;
    }
};
