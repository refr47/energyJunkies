#include "batteryMonitor.h"
#include "phaseController.h"
#include <cassert>
#include <iostream>

static unsigned long now = 0;
static int pins[4] = {};
unsigned long millis() { return now; }
void delay(unsigned long ms) { now += ms; }
void pinMode(int, int) {}
int digitalRead(int pin) { return pins[pin]; }
void digitalWrite(int pin, int value) { pins[pin] = value; }
void analogWrite(int pin, int value) { pins[pin] = value; }

int main() {
    PhaseController phase;
    phase.initPins(1, 2, 3);
    phase.setOnePhase(1000);
    LogEntry entry;
    // A target requested during relay dwell must eventually reach hardware,
    // even if the target remains constant.
    now = 2000;
    phase.apply(2000, entry);
    assert(phase.getHeaterPower() == 1000);
    now = 12000;
    phase.apply(2000, entry);
    assert(phase.getHeaterPower() == 2000);

    phase.allOff();
    now += 12000;
    // Slowly increasing targets must accumulate across the deadband.
    for (int target = 10; target <= 200; target += 10) {
        phase.apply(target, entry);
        now += 2000;
    }
    assert(phase.getHeaterPower() >= 170);

    phase.allOff();
    now += 12000;
    BatteryMonitor battery;
    bool bias = false;
    // Inverter holds grid at zero and redirects PV between battery and heater.
    // All 2000 W initially charge the battery. Mode 0 must claim that power.
    for (int cycle = 0; cycle < 60; ++cycle) {
        int heater = phase.getHeaterPower();
        // Meter updates every 40 s, control and hardware every 2 s.
        if (cycle % 20 == 0) {
            battery.update(0, -(2000 - heater), 20, 0, 0,
                           cycle / 20 + 1, heater);
        }
        int target = battery.getAvailablePower(bias);
        phase.apply(target, entry);
        now += 2000;
    }
    assert(phase.getHeaterPower() >= 1970);
    // An off/on cycle with the same target must not get stuck in deadband.
    phase.allOff();
    now += 12000;
    phase.apply(2000, entry);
    assert(phase.getHeaterPower() == 2000);
    std::cout << "Battery/phase control regression tests passed\n";
}
