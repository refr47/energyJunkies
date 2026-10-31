#include "energySource.h"
#include "app_state.h"

bool EnergySource::publishSample(WEBSOCK_DATA &data, ENERGY_STATE sample)
{
    if (!appLockData()) {
        return false;
    }
    // PID writes mCurrentPower while holding the same DATA lock.
    sample.heaterPowerAtSample = data.pidContainer.mCurrentPower;
    sample.sampleSequence = data.energyState.sampleSequence + 1;
    if (sample.sampleSequence == 0) {
        sample.sampleSequence = 1;
    }
    sample.sampledAt = millis();
    sample.valid = true;
    sample.saving = data.energyState.saving;
    sample.night = data.energyState.night;
    data.energyState = sample;
    appUnlockData();
    return true;
}
