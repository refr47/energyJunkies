// =============================================================================
// phaseController.cpp — PhaseController Implementation for OhmPilot
//
// Responsibilities:
//   - Relay/PWM toggling with hysteresis, min-dwell, and deadband logic
//   - currentPhases / currentPWM state tracking
//   - heaterPower() total draw calculation
//   - allOn() / allOff() hardware helpers
//
// GPIO calls are direct. All logic is self-contained and independent of
// temperature, battery, PV, or TinyNN.
// =============================================================================

#include "phaseController.h"

// Power constants (shared with pinManager)
#ifndef FASE_EINS_POWER
#define FASE_EINS_POWER 576     // W per phase (standard 3-phase heater)
#endif
#ifndef DEAD_BAND
#define DEAD_BAND 30
#endif
 
// ── Constructor ────────────────────────────────────────────────────────────
PhaseController::PhaseController()
    : pinL1(-1), pinL2(-1), pwmPin(-1),
      currentPhases(0), currentPWM(0),
      lastSwitchL1(0), lastSwitchL2(0), lastSwitchPwm(0),
      lastTargetPower(0), onePhase(FASE_EINS_POWER)
{
}

PhaseController::~PhaseController() = default;

// ── Initialization ────────────────────────────────────────────────────────
void PhaseController::initPins(int l1, int l2, int pwm)
{
    pinL1  = l1;
    pinL2  = l2;
    pwmPin = pwm;

    // Configure all relay + PWM pins as OUTPUT so digitalWrite/analogWrite work
    if (pinL1  > 0) pinMode(pinL1,  OUTPUT);
    if (pinL2  > 0) pinMode(pinL2,  OUTPUT);
    if (pwmPin > 0) pinMode(pwmPin, OUTPUT);
}

// ── pin read-back helper (static: no PinManager-state needed) ──────────────
bool PhaseController::isPinHigh(int pin)
{
    return (pin > 0) && (digitalRead(pin) == HIGH);
}

// ── toggleRelay (inline helper inside apply) ──────────────────────────────
// Returns true if HIGH, false if LOW.
 bool PhaseController::toggleRelay(int pin, bool wantOn, bool keepOn,
                       unsigned long &lastSwitch, unsigned long minSwitch,
                       unsigned long now)
{
    bool active = isPinHigh(pin);

    if (wantOn && !active)
    {
        if (now - lastSwitch >= minSwitch)
        { digitalWrite(pin, HIGH); lastSwitch = now; return true; }
    }
    else if (!wantOn && !keepOn && active)
    {
        if (now - lastSwitch >= minSwitch)
        { digitalWrite(pin, LOW);  lastSwitch = now; return false; }
    }
    return active;
}

// ──────────────────────────────────────────────────────────────────────────
// apply — translate power output into relay/PWM pin assignments + logging
//
// Step-up logic (3 phases):
//   Phase 1 (relay1)  if power >= onePhase
//   Phase 1+2 (relay2) if power >= 2*onePhase
//   Phase 3 (PWM)     proportional for any remainder above active relays
//
// HYSITERESIS:
//   - ON at ≥ threshold, OFF at ≤ (threshold - HYSTERESIS_WATT)
//   - MIN_SWITCH_L1/L2: 10s min dwell before relay toggle (on & off)
//   - MIN_SWITCH_PWM: 2s for PWM updates
//   - Small fluctuations within DEAD_BAND are ignored
// ──────────────────────────────────────────────────────────────────────────
void PhaseController::apply(int targetPower, LogEntry &logEntry)
{
    unsigned long now = millis();

    // ── Clamp power to valid range ────────────────────────────────────────
    int power = targetPower;
    if (power < 0)               power = 0;
    if (power > onePhase * 3)    power = onePhase * 3;

    LOG_INFO(TAG_PHASE, "apply() enter | target=%dW clamped=%dW lastTarget=%dW diff=%d DEADBAND=%d",
             targetPower, power, lastTargetPower, abs(power - lastTargetPower), DEAD_BAND);

    // ── Deadband: only evaluate hardware changes if difference exceeds ────
    int diff = abs(power - lastTargetPower);
    if (diff < DEAD_BAND)
    {
        LOG_INFO(TAG_PHASE, "DEADBAND skip | no hardware change");
        lastTargetPower = power;
        logEntry.power = power;
        logEntry.state = (uint8_t)MODE_AUTO;
        return;
    }

    lastTargetPower = power;

    // ── Hysteresis thresholds ────────────────────────────────────────────
    const int hyst1On  = onePhase;
    const int hyst1Off = onePhase - HYSTERESIS_WATT;
    const int hyst2On  = onePhase * 2;
    const int hyst2Off = onePhase * 2 - HYSTERESIS_WATT;

    // ── Toggle relays ────────────────────────────────────────────────────
    bool l1Active = toggleRelay(pinL1,
        power >= hyst1On,
        power > hyst1Off && power < hyst1On,
        lastSwitchL1, MIN_SWITCH_L1, now);

    bool l2Active = toggleRelay(pinL2,
        power >= hyst2On,
        power > hyst2Off && power < hyst2On,
        lastSwitchL2, MIN_SWITCH_L2, now);

    // Compute currentPhases from ACTUAL relay state
    currentPhases = (l1Active ? 1 : 0) + (l2Active ? 1 : 0);

    // ── PWM (Phase 3): Proportional remainder based on active relay power ─
    int relayPower = currentPhases * onePhase;
    int remainder  = power - relayPower;

    if (remainder <= 0)
    {
        // Active relays already cover the target → PWM off
        currentPWM = 0;
        if (pwmPin > 0)
        {
            analogWrite(pwmPin, 0);
            lastSwitchPwm = now;
        }
    }
    else
    {
        // Cap remainder at onePhase (PWM handles max 1 phase)
        if (remainder > onePhase) remainder = onePhase;
        currentPWM = (int)((remainder * OUTPUT_MAX) / (float)onePhase);
        if (pwmPin > 0)
        {
            analogWrite(pwmPin, (uint8_t)currentPWM);
            lastSwitchPwm = now;
        }
    }

    // ── Log fields ───────────────────────────────────────────────────────
    logEntry.power = power;
    logEntry.pwm  = (uint8_t)currentPWM;
    logEntry.state = (uint8_t)MODE_AUTO;

    int pwmWatt = (currentPWM * onePhase) / (float)OUTPUT_MAX;
    int totalWatt = currentPhases * onePhase + pwmWatt;
    LOG_INFO(TAG_PHASE, "APPLY result | L1=%s L2=%s | phases=%d PWM=%d/%.0f (%dW) | total=%dW",
             isPinHigh(pinL1) ? "ON" : "off",
             isPinHigh(pinL2) ? "ON" : "off",
             currentPhases, currentPWM, OUTPUT_MAX, pwmWatt, totalWatt);
}

// ──────────────────────────────────────────────────────────────────────────
// heaterPower — current heater draw in watts
// ──────────────────────────────────────────────────────────────────────────
int PhaseController::getHeaterPower() const
{
    int power = 0;
    power += (currentPhases) * onePhase;        // relays
    power += (currentPWM * onePhase) / (float)OUTPUT_MAX;  // PWM
    return power;
}

// ──────────────────────────────────────────────────────────────────────────
// allOn — force all phases HIGH (emergency / test / forceHeating)
// ──────────────────────────────────────────────────────────────────────────
void PhaseController::allOn()
{
    if (pinL1  > 0) digitalWrite(pinL1,  HIGH);
    if (pinL2  > 0) digitalWrite(pinL2,  HIGH);
    if (pwmPin > 0) analogWrite(pwmPin,  OUTPUT_MAX);

    currentPhases = 2;
    currentPWM    = (uint8_t)OUTPUT_MAX;
    lastSwitchL1  = millis();
    lastSwitchL2  = millis();
    lastSwitchPwm = millis();
}

// ──────────────────────────────────────────────────────────────────────────
// allOff — force all phases LOW (emergency / test / mode off)
// ──────────────────────────────────────────────────────────────────────────
void PhaseController::allOff()
{
    if (pinL1  > 0) digitalWrite(pinL1,  LOW);
    if (pinL2  > 0) digitalWrite(pinL2,  LOW);
    if (pwmPin > 0) analogWrite(pwmPin,  0);

    currentPhases = 0;
    currentPWM    = 0;
    lastSwitchL1  = millis();
    lastSwitchL2  = millis();
    lastSwitchPwm = millis();
}

// ──────────────────────────────────────────────────────────────────────────
// testPins — short pulse for initial pin-check (called once at boot)
// ──────────────────────────────────────────────────────────────────────────
void PhaseController::testPins(int testDurationMs)
{
    if (pwmPin > 0) analogWrite(pwmPin, OUTPUT_MAX);
    delay(testDurationMs / 4);
    if (pwmPin > 0) analogWrite(pwmPin, 0);

    if (pinL1 > 0) digitalWrite(pinL1, HIGH);
    delay(testDurationMs / 4);
    if (pinL1 > 0) digitalWrite(pinL1, LOW);

    if (pinL2 > 0) digitalWrite(pinL2, HIGH);
    delay(testDurationMs / 4);
    if (pinL2 > 0) digitalWrite(pinL2, LOW);

    delay(testDurationMs / 4);
    currentPhases = 0;
    currentPWM    = 0;
    lastSwitchL1  = millis();
    lastSwitchL2  = millis();
    lastSwitchPwm = millis();
}
