#pragma once
#include <stdint.h>
#include <cmath>

struct EnergyPollingConfig {
    uint32_t daySeconds = 40;
    uint32_t savingSeconds = 300;
    bool nightEnabled = true;
    unsigned int nightStart = 23;
    unsigned int nightEnd = 5;
    double lowPvWatts = 20;
    double wakePvWatts = 50;
    uint32_t holdSeconds = 300;
};
inline bool operator==(const EnergyPollingConfig &a, const EnergyPollingConfig &b) {
    return a.daySeconds == b.daySeconds && a.savingSeconds == b.savingSeconds
        && a.nightEnabled == b.nightEnabled && a.nightStart == b.nightStart
        && a.nightEnd == b.nightEnd && a.lowPvWatts == b.lowPvWatts
        && a.wakePvWatts == b.wakePvWatts && a.holdSeconds == b.holdSeconds;
}
inline uint32_t energyMaxAge(const EnergyPollingConfig &c, bool saving) {
    return 2000U * (saving ? c.savingSeconds : c.daySeconds);
}
// Equal start/end disables the window, including overnight windows.
inline bool energyNightHour(int hour, int start = 23, int end = 5) {
    if (hour < 0 || hour > 23 || start == end) return false;
    return start < end ? hour >= start && hour < end : hour >= start || hour < end;
}
class EnergyPollingPolicy {
    EnergyPollingConfig config;
    uint32_t lastSequence = 0;
    uint32_t lowSince = 0;
    bool trackingLow = false;
    bool saving = false;
public:
    bool configure(const EnergyPollingConfig &next) {
        if (next == config) return false;
        config = next;
        lastSequence = 0;
        lowSince = 0;
        trackingLow = saving = false;
        return true;
    }
    bool isSaving() const { return saving; }
    uint32_t update(uint32_t now, bool night, bool valid, uint32_t sequence, double pv) {
        if (night || !valid || !std::isfinite(pv)) {
            trackingLow = saving = false;
            lastSequence = sequence;
            return night ? 0 : config.daySeconds * 1000U;
        }
        if (sequence != lastSequence) {
            lastSequence = sequence;
            if (pv > config.wakePvWatts) {
                saving = trackingLow = false;
            } else if (!saving) {
                if (pv < config.lowPvWatts) {
                    if (!trackingLow) { lowSince = now; trackingLow = true; }
                    if (uint32_t(now - lowSince) >= config.holdSeconds * 1000U) saving = true;
                } else trackingLow = false;
            }
        }
        return 1000U * (saving ? config.savingSeconds : config.daySeconds);
    }
};
