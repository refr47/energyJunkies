#ifndef PHASECONTROLLER_H
#define PHASECONTROLLER_H

#include <Arduino.h>

#include "defines.h"
//#include "dataTypes.hpp"



class PinManager;





const double OUTPUT_MAX = 255.0;

// =============================================================================
// PhaseController — relay/PWM hardware steering + logging interface
//
// Encapsulates:
//   - Relay toggling with hysteresis + min-dwell (toggleRelay helper)
//   - Power clamping + deadband check
//   - PWM proportional remainder for active phase power
//   - currentPhase/currentPWM state (read by pidLockWrite → WebSocket)
//   - heaterPower() — total draw calculation
//
// Designed to be testable without hardware: all GPIO calls are direct but
// the logic is self-contained and independent of temperature / battery / PV.
// =============================================================================
class PhaseController
{
    friend class PinManager;

public:
    PhaseController();
    ~PhaseController();

    // ── Initialization ────────────────────────────────────────────────────
    void initPins(int l1, int l2, int pwm);
    void setOnePhase(int w) { onePhase = w; }

    // ── Hardware apply ────────────────────────────────────────────────────
    // Translate target power → relay + PWM outputs, fill log fields.
    void apply(int targetPower, LogEntry& logEntry);

    // ── State queries (read by caller, thread-safe after apply) ────────────
    int  getOnePhase()    const { return onePhase; }
    int  getActivePhases() const { return currentPhases; }
    int  getPwm()         const { return currentPWM; }
    int  getHeaterPower() const;

    // ── Test / Force helpers ──────────────────────────────────────────────
    void allOn();
    void allOff();
    void testPins(int testDurationMs = 3000);

private:
    // ── Relay state ──────────────────────────────────────────────────────
    int  pinL1 = -1, pinL2 = -1, pwmPin = -1;
    int  currentPhases = 0;
    int  currentPWM = 0;

    // ── Timing ────────────────────────────────────────────────────────────
    unsigned long lastSwitchL1 = 0;
    unsigned long lastSwitchL2 = 0;
    unsigned long lastSwitchPwm = 0;
    int           lastTargetPower = 0;

    // ── Config (set once, never changed) ──────────────────────────────────
    int    onePhase;
    const unsigned long MIN_SWITCH_L1 = 10000; // 10s Hysterese Relay 1
    const unsigned long MIN_SWITCH_L2 = 10000; // 10s Hysterese Relay 2
    const unsigned long MIN_SWITCH_PWM = 2000;  // 2s PWM

    // ── Pin read-back helper (static: no PinManager-state needed) ──────────
    bool isPinHigh(int pin);
    bool toggleRelay(int pin, bool wantOn, bool keepOn,
        unsigned long& lastSwitch, unsigned long minSwitch,
        unsigned long now);
};

#endif // PHASECONTROLLER_H
