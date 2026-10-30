// Host regression: compile against the actual BatteryMonitor implementation.
#include "batteryMonitor.h"
#include <cassert>
#include <iostream>

int power(float grid, float batt, int priority, int heater = 0, float soc = 80) {
    BatteryMonitor monitor;
    monitor.update(grid, batt, soc, priority, 0, 1, heater);
    bool bias = true;
    int result = monitor.getAvailablePower(bias);
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
    uint32_t sequence = 0;
    monitor.update(-2000, 0, 80, 1, 0, ++sequence, 0);
    assert(monitor.getAvailablePower(bias) == 2000);
    monitor.update(-100, 100, 80, 1, 0, ++sequence, 0);
    assert(monitor.getAvailablePower(bias) == 0);
    monitor.update(-100, 0, 80, 1, 0, ++sequence, 0);
    assert(monitor.getAvailablePower(bias) == 100); // no stale EMA
    monitor.update(100, 0, 80, 1, 0, ++sequence, 0);
    assert(monitor.getAvailablePower(bias) == 0); // immediate reduction
    monitor.update(-500, -1000, 80, 0, 0, ++sequence, 0);
    assert(monitor.getAvailablePower(bias) == 1500);
    assert(monitor.isBatteryCharging()); // measurement was preserved
    assert(monitor.isBatteryOK());
    monitor.update(-500, -1000, 80, 1, 0, ++sequence, 0);
    assert(monitor.getAvailablePower(bias) == 500); // mode resets EMA
    monitor.update(-500, 0, 80, 1, 1234, ++sequence, 0);
    assert(monitor.getAvailablePower(bias) == 1234 && bias);
    monitor.update(-500, 100, 80, 1, 1234, ++sequence, 0);
    assert(monitor.getAvailablePower(bias) == 0 && !bias);
    assert(!monitor.isBatteryOK());
    // Reproduce the logs: a 40-second-old reading must not be combined
    // with heater changes made every 2 seconds.
    BatteryMonitor logged;
    logged.update(-4338, -2, 100, 0, 0, 1, 0);
    assert(logged.getAvailablePower(bias) == 4340);
    for (int tick = 0; tick < 20; ++tick) {
        logged.update(-4338, -2, 100, 0, 0, 1, 0);
        assert(logged.getAvailablePower(bias) == 4340);
    }
    logged.update(911, -3, 100, 0, 0, 2, 4500);
    assert(logged.getAvailablePower(bias) == 3592);
    for (int tick = 0; tick < 20; ++tick) {
        logged.update(911, -3, 100, 0, 0, 2, 4500);
        assert(logged.getAvailablePower(bias) == 3592);
    }
    // EMA advances once per successful sample, including identical readings.
    BatteryMonitor smoothing;
    smoothing.update(0, 0, 100, 0, 0, 1, 0);
    assert(smoothing.getAvailablePower(bias) == 0);
    smoothing.update(-1000, 0, 100, 0, 0, 2, 0);
    assert(smoothing.getAvailablePower(bias) == 150);
    for (int tick = 0; tick < 20; ++tick) {
        smoothing.update(-1000, 0, 100, 0, 0, 2, 0);
        assert(smoothing.getAvailablePower(bias) == 150);
    }
    smoothing.update(-1000, 0, 100, 0, 0, 3, 0);
    assert(smoothing.getAvailablePower(bias) == 277);
    // Setup changes must reuse the original heater reference, not current draw.
    smoothing.update(-1000, 0, 100, 1, 0, 3, 0);
    assert(smoothing.getAvailablePower(bias) == 1000);
    smoothing.update(-1000, 0, 100, 1, 1234, 3, 0);
    assert(smoothing.getAvailablePower(bias) == 1234 && bias);
    assert(smoothing.getAvailablePower(bias) == 1234 && bias);
    smoothing.update(-1000, 0, 100, 1, 0, 3, 0);
    assert(smoothing.getAvailablePower(bias) == 1000 && !bias);
    BatteryMonitor notReady;
    notReady.update(0, 0, 0, 0, 1234, 0, 0);
    assert(notReady.getAvailablePower(bias) == 0 && !bias);
    std::cout << "BatteryMonitor regression tests passed\n";
}
