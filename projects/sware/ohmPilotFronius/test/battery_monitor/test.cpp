// Host regression: compile against the actual BatteryMonitor implementation.
#include "batteryMonitor.h"
#include <cassert>
#include <iostream>

int power(float grid, float batt, int priority, int heater = 0, float soc = 80) {
    BatteryMonitor monitor;
    monitor.update(grid, batt, soc, priority, 0);
    bool bias = true;
    int result = monitor.getAvailablePower(heater, bias);
    assert(!bias);
    return result;
}

int main() {
    assert(power(-500, -1000, 0) == 1500);
    assert(power(-500, -1000, 1) == 500);
    assert(power(-500, -1000, 2) == 500);
    for (int mode = 0; mode <= 2; ++mode) {
        assert(power(-500, 0, mode, 1000) == 1500);
        assert(power(0, 0, mode, 1000) == 1000);
        assert(power(500, 0, mode, 1000) == 500);
        assert(power(1500, 0, mode, 1000) == 0);
    }
    assert(power(-500, 100, 0, 1000) == 0);
    assert(power(-500, 100, 1, 1000) == 0);
    assert(power(0, 1000, 2, 1000, 20) == 1000);
    assert(power(0, 1000, 2) == 0); // discharge is not extra surplus
    assert(power(-500, 30, 1) == 500);
    assert(power(-500, 31, 1) == 0);
    assert(power(-500, -1000, 1, 0, 39) == 0);
    assert(power(-500, -1000, 1, 0, 40) == 500);
    assert(power(-500, 0, 0, 0, 0) == 500); // no battery

    BatteryMonitor monitor;
    bool bias = false;
    monitor.update(-2000, 0, 80, 1, 0);
    assert(monitor.getAvailablePower(0, bias) == 2000);
    monitor.update(-100, 100, 80, 1, 0);
    assert(monitor.getAvailablePower(0, bias) == 0);
    monitor.update(-100, 0, 80, 1, 0);
    assert(monitor.getAvailablePower(0, bias) == 100); // no stale EMA
    monitor.update(100, 0, 80, 1, 0);
    assert(monitor.getAvailablePower(0, bias) == 0); // immediate reduction
    monitor.update(-500, -1000, 80, 0, 0);
    assert(monitor.getAvailablePower(0, bias) == 1500);
    assert(monitor.isBatteryCharging()); // measurement was preserved
    assert(monitor.isBatteryOK());
    monitor.update(-500, -1000, 80, 1, 0);
    assert(monitor.getAvailablePower(0, bias) == 500); // mode resets EMA
    monitor.update(-500, 0, 80, 1, 1234);
    assert(monitor.getAvailablePower(0, bias) == 1234 && bias);
    monitor.update(-500, 100, 80, 1, 1234);
    assert(monitor.getAvailablePower(0, bias) == 0 && !bias);
    assert(!monitor.isBatteryOK());
    std::cout << "BatteryMonitor regression tests passed\n";
}
