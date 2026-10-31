#include "energyPolling.h"
#include <cassert>
#include <limits>
#include <iostream>
int main() {
    assert(energyNightHour(23)); assert(energyNightHour(0));
    assert(energyNightHour(4)); assert(!energyNightHour(5));
    assert(!energyNightHour(22)); assert(!energyNightHour(-1));
    assert(!energyNightHour(12, 5, 5));
    assert(energyNightHour(12, 10, 14)); assert(!energyNightHour(14, 10, 14));
    EnergyPollingPolicy p;
    assert(p.update(0, false, true, 1, 19) == ENERGY_DAY_INTERVAL);
    // Repeated scheduler calls with the same sample cannot establish low PV.
    assert(p.update(300000, false, true, 1, 19) == ENERGY_DAY_INTERVAL);
    assert(p.update(300000, false, true, 2, 19) == ENERGY_NIGHT_INTERVAL);
    assert(p.isSaving());
    assert(p.update(600000, false, true, 3, 50) == ENERGY_NIGHT_INTERVAL);
    assert(p.update(900000, false, true, 4, 51) == ENERGY_DAY_INTERVAL);
    assert(!p.isSaving());
    p.update(1000000, false, true, 5, 0);
    p.update(1299999, false, true, 6, 20); // exact 20 resets hold
    assert(p.update(1300000, false, true, 7, 0) == ENERGY_DAY_INTERVAL);
    assert(p.update(1400000, false, false, 7, 0) == ENERGY_DAY_INTERVAL);
    assert(p.update(1600000, false, true, 8, 0) == ENERGY_DAY_INTERVAL);
    assert(p.update(1900000, false, true, 9, 0) == ENERGY_NIGHT_INTERVAL);
    assert(p.update(2000000, true, true, 9, 0) == 0);
    assert(p.update(2100000, false, false, 9, 0) == ENERGY_DAY_INTERVAL);
    assert(!p.isSaving());
    assert(p.update(2200000, false, true, 10, std::numeric_limits<double>::quiet_NaN()) == ENERGY_DAY_INTERVAL);
    EnergyPollingPolicy wrap;
    const uint32_t start = UINT32_MAX - 100000;
    wrap.update(start, false, true, 1, 0);
    assert(wrap.update(start + uint32_t(300000), false, true, 2, 0) == ENERGY_NIGHT_INTERVAL);
    std::cout << "Energy polling tests passed\n";
}
