// =============================================================================
// pinManager.cpp — PinManager Implementation for OhmPilot
//
// Thread-Safety Notes:
// - update() is called with BOTH appLockData() + appLockPidOut() held
//   → safe to read g_app.webSockData AND write g_app.pinManager.state
// - pidLockRead() copies shared data → local member vars
// - pidLockWrite() writes local output → shared pidContainer
// - Logging to RingBuffer uses its own mutex (utils_logWrite)
//
// Architecture:
// 1. Forced modes (OFF, L1, L1+L2, L1+L2+PWM, Legionella) — direct pin control
// 2. Auto mode — PID-like controller using PV surplus data
// 3. NO compensation: smart meter reads include heater → no feedback loop
// 4. Battery priority: charger first, boiler only on excess
// 5. Phase-safe: relays only ON when full phase-power available
// =============================================================================
 
#include "utils.h"
#include "pinManager.h"

// ── Constants (legacy, used by original code) ──
#define FASE_EINS_POWER 576     // W per phase (standard 3-phase heater)
#define PUFFER_AKKU_PRIORI 50   // Battery SoC threshold for priority
#define RAMP_DOWN_STEP 100      // Max W decrease per ramp interval
#define RAMP_DOWN_INTERVAL 1000 // ms between ramp steps

// ──────────────────────────────────────────────────────────────────────────
// Constructor
// ──────────────────────────────────────────────────────────────────────────
PinManager::PinManager()
{
    pinL1 = 0;
    pinL2 = 0;
    pwmPin = 0;
    onePhase = 0;
    currentPhases = 0;
    currentPWM = 0;
    lastLegionella = 0;
    legionella = false;
    availablePower.clear();
    powerIndex = 0;
    rest = 0;

    m_sensor1 = 0;
    m_sensor2 = 0;
    m_froniusAPI = false;
    m_boilerHeating = false;
    m_tempMaxAllowed = 60;
    m_tempMin = 45;
    m_legionellenMaxTemp = 0;
    m_legionellenDelta = 0;
    m_akkuPriori = 0;
    m_gridPower = 0;
    m_meterPower = 0;
    m_pV = 0;
    m_soc = 0;
    m_battPower = 0;
    m_wattSetupForTest = 0;
    m_forceHeating = 0;

    m_out_boilerHeating = false;
    m_out_wattBiasForTest = false;

    m_weatherBonus = 0;
    m_weatherPvRatio = 1.0f;
    m_weatherCloudAvg = 0.5f;
    m_weatherOutTemp = 20.0f;

#ifdef TINYNN_ENABLE
    tinyNN = nullptr;
    m_tinyNN_preheat = 0;
    m_tinyNN_buffer = 0;
#endif
}

// ──────────────────────────────────────────────────────────────────────────
// Destructor
// ──────────────────────────────────────────────────────────────────────────
PinManager::~PinManager()
{
#ifdef TINYNN_ENABLE
    if (tinyNN != nullptr)
    {
        delete tinyNN;
        tinyNN = nullptr;
    }
#endif
}

// ──────────────────────────────────────────────────────────────────────────
// CONFIG — initialize pin assignments and hardware parameters
//
// Called at startup from main.cpp with pin numbers + WEBSOCK_DATA.
// Reads power/setup from data, configures pin directions.
// ──────────────────────────────────────────────────────────────────────────
void PinManager::config(WEBSOCK_DATA &data, int l1, int l2, int pwm)
{
    pinL1 = l1;
    pinL2 = l2;
    pwmPin = pwm;

    // Total heater power from setup (3-phase), onePhase = per-phase share
    float totalPower = (float)data.setupData.heizstab_leistung_in_watt;
    onePhase = (totalPower > 0.0f) ? (int)(totalPower / 3.0f) : FASE_EINS_POWER;

    // Pin directions
    pinMode(pinL1, OUTPUT);
    pinMode(pinL2, OUTPUT);
    pinMode(pwmPin, OUTPUT);

    // Enable PWM on phase 3 pin
    analogWrite(pwmPin, 0);

    // Default: relays LOW = OFF (SSRs)
    digitalWrite(pinL1, LOW);
    digitalWrite(pinL2, LOW);

#ifdef TINYNN_ENABLE
    // Initialize TinyNN if enabled
    if (tinyNN == nullptr)
    {
        tinyNN = new TinyNN((float)onePhase * 3.0f,
                            (float)data.setupData.tempMinInGrad,
                            (float)data.setupData.tempMaxAllowedInGrad);
        tinyNN->load();
    }
#endif

    LOG_INFO(TAG_PID,
             "PinManager configured: relay1=%d relay2=%d pwm=%d onePhase=%dW maxTemp=%d minTemp=%d",
             l1, l2, pwm, onePhase,
             data.setupData.tempMaxAllowedInGrad,
             data.setupData.tempMinInGrad);
}

// ──────────────────────────────────────────────────────────────────────────
// testPins — hardware verification blip (350ms HIGH, then LOW)
// ──────────────────────────────────────────────────────────────────────────
void PinManager::testPins(int l1, int l2, int pwm)
{
    int pins[] = {l1, l2, pwm};
    for (int i = 0; i < 3; i++)
    {
        if (pins[i] > 0)
        {
            pinMode(pins[i], OUTPUT);
            digitalWrite(pins[i], HIGH);
            delay(350);
            digitalWrite(pins[i], LOW);
        }
    }
    if (pwm > 0)
        analogWrite(pwm, 0);
}

// ──────────────────────────────────────────────────────────────────────────
// reset — clear all state, turn off pins, clear targets
// ──────────────────────────────────────────────────────────────────────────
void PinManager::reset()
{
    LOG_INFO(TAG_PID, "PinManager reset() called. Heater off.");

    currentPhases = 0;
    currentPWM = 0;
    rest = 0;
    lastTargetPower = 0;
    lastSmoothedPower = 0;
    lastSwitch = 0;
    legionella = false;
    m_froniusAPI = false;
    m_boilerHeating = false;
    m_out_boilerHeating = false;
    m_out_wattBiasForTest = false;

    if (pinL1 > 0)  digitalWrite(pinL1, LOW);
    if (pinL2 > 0)  digitalWrite(pinL2, LOW);
    if (pwmPin > 0) analogWrite(pwmPin, 0);
}

// ──────────────────────────────────────────────────────────────────────────
// allOn — emergency max-power (used by legio / min-temp / test)
// ──────────────────────────────────────────────────────────────────────────
void PinManager::allOn()
{
    unsigned long now = millis();
    if (now - lastSwitch < MIN_SWITCH)
    {
        unsigned long wait = MIN_SWITCH - (now - lastSwitch);
        if (wait > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(wait));
        }
    }

    if (pinL1 > 0)  digitalWrite(pinL1, HIGH);
    if (pinL2 > 0)  digitalWrite(pinL2, HIGH);
    if (pwmPin > 0) analogWrite(pwmPin, (uint8_t)OUTPUT_MAX);

    currentPhases = 3;
    currentPWM = (int)OUTPUT_MAX;
    lastSwitch = millis();
}

// ──────────────────────────────────────────────────────────────────────────
// getStateOfDigPin / getStateOfAnaPin — read-back hardware state
// ──────────────────────────────────────────────────────────────────────────
int PinManager::getStateOfDigPin(short pin)
{
    return digitalRead(pin);
}

int PinManager::getStateOfAnaPin()
{
    if (pwmPin > 0)
        return analogRead(pwmPin);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────
// apply — translate power output into relay/PWM pin assignments + logging
//
// Step-up logic (3 phases):
//   Phase 1 (relay1)  if power > 0
//   Phase 1+2 (relay2) if power > onePhase
//   Phase 3 (PWM)     proportional for any remainder above 2x onePhase
//
// DEAD BAND:
//   - Small fluctuations within DEAD_BAND are ignored
//     (prevents micro-hunting at the step boundaries)
// ──────────────────────────────────────────────────────────────────────────
void PinManager::apply(LogEntry &logEntry, int power)
{
    unsigned long now = millis();

    // Clamp power to valid range
    int clampedPower = power;
    if (clampedPower < 0)               clampedPower = 0;
    if (clampedPower > onePhase * 3)    clampedPower = onePhase * 3;

    // Deadband: only change hardware if difference exceeds threshold
    int diff = abs(clampedPower - lastTargetPower);
    if (diff < DEAD_BAND)
    {
        // Still update the log entry with target power, but don't change pins
        lastTargetPower = clampedPower;
        logEntry.power = clampedPower;
        logEntry.state = (uint8_t)MODE_AUTO;
        return;
    }

    lastTargetPower = clampedPower;

    // ── Phase assignment logic ──
    // CRITICAL: Only turn relays ON when there's enough surplus power
    // to feed the full phase. Otherwise use PWM on phase 3.
    //
    // One phase = onePhase watts (~576W).
    // If availablePower < onePhase → PWM proportional, relays OFF
    // If availablePower >= onePhase → Relay 1 ON
    // If availablePower >= 2*onePhase → Relay 2 ON
    // PWM always handles the remainder proportionally

    if (clampedPower <= 0)
    {
        // OFF
        currentPhases = 0;
        currentPWM = 0;
        if (pinL1 > 0)  digitalWrite(pinL1, LOW);
        if (pinL2 > 0)  digitalWrite(pinL2, LOW);
        if (pwmPin > 0) analogWrite(pwmPin, 0);
        lastSwitch = now;
        return;
    }

    // ── Phase 1 (Relay 1): Only ON if enough power for full phase ──
    if (clampedPower >= onePhase)
    {
        if (pinL1 > 0 && now - lastSwitch >= MIN_SWITCH)
        {
            digitalWrite(pinL1, HIGH);
            lastSwitch = now;
        }
    }
    else if (pinL1 > 0)
    {
        digitalWrite(pinL1, LOW);  // Not enough power → relay off
    }

    // ── Insufficient power for full phase → PWM only ──
    if (clampedPower < onePhase)
    {
        currentPhases = 0;
        currentPWM = (int)((clampedPower * OUTPUT_MAX) / onePhase);
        if (pwmPin > 0)
            analogWrite(pwmPin, (uint8_t)currentPWM);
        if (pinL2 > 0)  digitalWrite(pinL2, LOW);
        return;
    }

    // ── Phase 2 (Relay 2): Only ON if enough power for 2nd phase ──
    if (clampedPower >= onePhase * 2)
    {
        if (pinL2 > 0 && now - lastSwitch >= MIN_SWITCH)
        {
            digitalWrite(pinL2, HIGH);
            lastSwitch = now;
        }
    }
    else if (pinL2 > 0)
    {
        digitalWrite(pinL2, LOW);  // Not enough power for 2nd phase → off
    }

    // ── PWM (Phase 3): Proportional remainder ──
    int remainder = clampedPower - (currentPhases * onePhase);
    if (remainder > 0)
    {
        currentPWM = (int)((remainder * OUTPUT_MAX) / onePhase);
        if (pwmPin > 0)
            analogWrite(pwmPin, (uint8_t)currentPWM);
    }
    else
    {
        currentPWM = 0;
        if (pwmPin > 0)
            analogWrite(pwmPin, 0);
    }

    // Update currentPhases count
    if (clampedPower >= onePhase * 2)
        currentPhases = 2;
    else
        currentPhases = 1;
}

// ──────────────────────────────────────────────────────────────────────────
// pidLockRead — copy shared data into local members (thread-safe under lock)
//
// Called from update() while BOTH data + pidOut locks are held.
// Copies PV/grid/temperature data from WEBSOCK_DATA → private members.
// ──────────────────────────────────────────────────────────────────────────
void PinManager::pidLockRead(WEBSOCK_DATA &webSockData)
{
    m_sensor1 = webSockData.temperature.sensor1;
    m_sensor2 = webSockData.temperature.sensor2;
    m_froniusAPI = webSockData.states.froniusAPI;
    m_boilerHeating = webSockData.states.boilerHeating;
    m_tempMaxAllowed = webSockData.setupData.tempMaxAllowedInGrad;
    m_tempMin = webSockData.setupData.tempMinInGrad;
    m_legionellenMaxTemp = webSockData.setupData.legionellenMaxTemp;
    m_legionellenDelta = webSockData.setupData.legionellenDelta;
    m_akkuPriori = (webSockData.setupData.akkuPriori != 0);
    m_forceHeating = webSockData.setupData.forceHeating;

    // Energy state
    if (m_froniusAPI)
    {
        m_gridPower = webSockData.fronius_SOLAR_POWERFLOW.p_grid;
        m_pV = webSockData.fronius_SOLAR_POWERFLOW.p_pv;
        m_soc = webSockData.fronius_SOLAR_POWERFLOW.soc;
        m_battPower = webSockData.fronius_SOLAR_POWERFLOW.p_batt;
        m_meterPower = webSockData.fronius_SOLAR_POWERFLOW.p_grid;
    }
#ifdef AMIS_READER_DEV
    else
    {
        m_gridPower = (float)webSockData.amisReader.exportInWatt;
        m_pV = (float)webSockData.amisReader.saldo;
        m_meterPower = (float)webSockData.amisReader.saldo;
        m_soc = 0;
        m_battPower = 0;
    }
#endif

    m_wattSetupForTest = webSockData.setupData.wattSetupForTest;
}

// ──────────────────────────────────────────────────────────────────────────
// pidLockWrite — write local output back to shared pidContainer
//
// Called from update() while BOTH data + pidOut locks are held.
// Updates pidContainer (read by webSocket JSON builder) and states.
// ──────────────────────────────────────────────────────────────────────────
void PinManager::pidLockWrite(WEBSOCK_DATA &webSockData)
{
    webSockData.pidContainer.mCurrentPower = (currentPhases * onePhase);
    if (currentPWM > 0)
    {
        webSockData.pidContainer.mCurrentPower +=
            (int)((currentPWM / OUTPUT_MAX) * onePhase);
    }

    webSockData.pidContainer.PID_PIN1 = (currentPhases >= 1);
    webSockData.pidContainer.PID_PIN2 = (currentPhases >= 2);
    webSockData.pidContainer.mAnalogOut = currentPWM;

    // Weather + TinyNN prediction fields
    webSockData.pidContainer.weatherBonus = m_weatherBonus;
    webSockData.pidContainer.weatherPvRatio = m_weatherPvRatio;
    webSockData.pidContainer.weatherCloudAvg = m_weatherCloudAvg;
    webSockData.pidContainer.weatherOutTemp = m_weatherOutTemp;

    // Boiler heating status
    m_out_boilerHeating = (currentPhases > 0 || currentPWM > 0);
    webSockData.states.boilerHeating = m_out_boilerHeating;

#ifdef TINYNN_ENABLE
    webSockData.pidContainer.tinyNN_preheat_score = m_tinyNN_preheat;
    webSockData.pidContainer.tinyNN_buffer_pct = m_tinyNN_buffer;
#endif
}

// ──────────────────────────────────────────────────────────────────────────
// resolveInputTemp — compute average temperature from local copies
// ──────────────────────────────────────────────────────────────────────────
int PinManager::resolveInputTemp() const
{
    int s1 = m_sensor1;
    int s2 = m_sensor2;

    // Handle sensor fail (negative = bad reading)
    if (s1 < 0 && s2 < 0)
        return m_tempMaxAllowed; // safe: above max → heater off
    if (s1 < 0)
        return s2;
    if (s2 < 0)
        return s1;

    return (s1 + s2) / 2;
}

// ──────────────────────────────────────────────────────────────────────────
// getMeanOfAvailAblePower — PV surplus for control
//
// getMeanOfAvailAblePower — PV surplus for ONE phase
//
// Smart meter measures grid power at the connection point:
//   p_grid < 0 → Einspeisung (Überschuss)
//   p_grid > 0 → Bezug (Defizit)
//
// NO heater compensation: The smart meter already includes heater draw.
// If meter shows -200W, then 200W of surplus IS available.
// As the heater turns on, this value naturally drops → next cycle
// will see less surplus → controller adjusts automatically.
//
// Battery priority:
//   - SOC < threshold → heater OFF (charge battery first)
//   - SOC >= threshold → heater allowed
// ──────────────────────────────────────────────────────────────────────────
int PinManager::getMeanOfAvailAblePower()
{
    // Measured surplus (negative grid = surplus feeding into grid)
    // p_grid < 0 = Einspeisung (Überschuss), p_grid > 0 = Bezug (Defizit)
    float measuredSurplus = -m_meterPower;

    // Only proceed if there's actual PV surplus
    if (measuredSurplus <= 0)
    {
        return 0;  // Kein Überschuss → Boiler aus
    }

    // Battery priority logic:
    // - Akku laden (p_batt < 0) + SOC < threshold → Boiler AUS, nur Akku laden
    // - Akku voll (> threshold) → Boiler darf vollen Überschuss nutzen
    // - Kein Überschuss → Boiler aus
    float availableSurplus = measuredSurplus;
    if (m_akkuPriori)
    {
        int battThreshold = PUFFER_AKKU_PRIORI;
        if ((int)m_soc < battThreshold)
        {
            return 0;  // SOC zu niedrig → Boiler aus, Akku zuerst laden
        }
    }

    // Scale: available power for ONE phase (3-phase system)
    int availablePower = (int)(availableSurplus / 3.0f);

    // Watt bias for testing (if wattSetupForTest is set, use fixed value)
    if (m_wattSetupForTest > 0)
    {
        availablePower = m_wattSetupForTest;
        m_out_wattBiasForTest = true;
    }

    return availablePower;
}

// ──────────────────────────────────────────────────────────────────────────
// heaterPower — current heater draw in watts
// ──────────────────────────────────────────────────────────────────────────
int PinManager::heaterPower()
{
    int power = 0;

    // Relays: each contributes onePhase watts
    power += (currentPhases) * onePhase;

    // PWM: fractional contribution
    if (currentPWM > 0)
    {
        power += (int)((currentPWM * onePhase) / OUTPUT_MAX);
    }

    return power;
}

// ──────────────────────────────────────────────────────────────────────────
// basePower — compute base target power from PV surplus
// ──────────────────────────────────────────────────────────────────────────
int PinManager::basePower(int effectivePower)
{
    // Clamp to valid range
    int maxPower = onePhase * 3;
    if (effectivePower < 0)   return 0;
    if (effectivePower > maxPower) return maxPower;

    return effectivePower;
}

// ──────────────────────────────────────────────────────────────────────────
// tempState / pvState — discretized state indices (for original RL design)
// ──────────────────────────────────────────────────────────────────────────
int PinManager::tempState(double t)
{
    int temp = (int)t;
    if (temp < m_tempMin)
        return 0;
    if (temp > m_tempMaxAllowed)
        return 1;
    // Map [tempMin, tempMax] → [0, 1]
    double range = (double)(m_tempMaxAllowed - m_tempMin);
    if (range <= 0)
        range = 1.0;
    return (int)((temp - m_tempMin) / range);
}

int PinManager::pvState(double p)
{
    int power = (int)p;
    int maxPower = onePhase * 3;
    if (power <= 0)
        return 0;
    if (power >= maxPower)
        return 1;
    // Map [0, maxPower] → [0, 1]
    if (maxPower <= 0)
        maxPower = 1;
    return (int)(power / (double)maxPower);
}

// ──────────────────────────────────────────────────────────────────────────
// preCheck — determine control mode before main loop
//
// Sets mode based on temperature + legionella state:
//   MODE_LEGIONELLA — if legio cycle is active and temp reached
//   MODE_OFF — if temp at or above max (including weather bonus)
//   MODE_MIN_TEMP — if temp below min (frost protection)
//   MODE_AUTO — normal PID control
// ──────────────────────────────────────────────────────────────────────────
void PinManager::preCheck(int temp, unsigned long nowMS,
                          ControlMode &mode, bool &doML)
{
    doML = false;
    mode = MODE_AUTO;

    // Effective max temp includes weather bonus
    int effectiveMax = (int)(m_tempMaxAllowed + m_weatherBonus);

    // ── Legionella cycle handling ──
    if ((m_legionellenMaxTemp > 0) && (m_legionellenDelta > 0))
    {
        // Check if legio cycle should start
        if (legionella)
        {
            // Already in legio mode — check if temp reached
            if (temp >= m_legionellenMaxTemp)
            {
                mode = MODE_LEGIONELLA;
                doML = false; // No ML during legio
                return;
            }
        }
        else
        {
            // Check if enough time passed for next legio cycle
            unsigned long legioDeltaMs = m_legionellenDelta * 3600000UL; // hours→ms
            if (nowMS - lastLegionella >= legioDeltaMs)
            {
                // Time for legio cycle
                mode = MODE_LEGIONELLA;
                lastLegionella = nowMS;
                doML = false;
                return;
            }
        }
    }

    // ── Max temp: heater off ──
    if (temp >= effectiveMax)
    {
        mode = MODE_OFF;
        legionella = false; // cancel legio if max temp hit
        return;
    }

    // ── Min temp: full power (frost protection) ──
    if (temp < m_tempMin)
    {
        mode = MODE_MIN_TEMP;
        return;
    }

    // ── Normal auto mode ──
    mode = MODE_AUTO;
    doML = true;
}

// ──────────────────────────────────────────────────────────────────────────
// handleModeOff — max-temp reached; heater off, log state
// ──────────────────────────────────────────────────────────────────────────
void PinManager::handleModeOff(LogEntry &logEntry)
{
    apply(logEntry, 0);

    currentPhases = 0;
    currentPWM = 0;
    m_out_boilerHeating = false;

    if (pinL1 > 0)  digitalWrite(pinL1, LOW);
    if (pinL2 > 0)  digitalWrite(pinL2, LOW);
    if (pwmPin > 0) analogWrite(pwmPin, 0);

    logEntry.state = (uint8_t)MODE_OFF;
    logEntry.power = 0;
    logEntry.pwm = 0;
}

// ──────────────────────────────────────────────────────────────────────────
// handleForceHeating — forced mode handling (from setupData.forceHeating)
// ──────────────────────────────────────────────────────────────────────────
void PinManager::handleForceHeating(LogEntry &logEntry, int &measuredPower)
{
    // This method handles the forced heating modes
    // measuredPower is passed by reference for logging
    measuredPower = getMeanOfAvailAblePower();
    logEntry.power = measuredPower;
}

// ──────────────────────────────────────────────────────────────────────────
// handleModeManual — manual/forced mode (not RL-controlled)
// ──────────────────────────────────────────────────────────────────────────
void PinManager::handleModeManual()
{
    // Manual mode: maintain current settings, no control update
    // Override handled in update() via forceHeating setup
}

// ──────────────────────────────────────────────────────────────────────────
// handleModeAuto — PID-like controller for automatic mode
//
// Control Loop:
//   1. Read actual PV surplus from smart meter (NO compensation)
//   2. Compute target power (with deadband)
//   3. Smooth downward transitions (rate-limiter / cloud-cover handling)
//   4. Apply hardware outputs via apply()
//   5. Populate LogEntry
//
// Battery priority: Boiler only heats if SOC >= threshold
// Phase safety: Relays only ON for full phase-power, PWM for partial
//
// Returns: 0 on success
// ──────────────────────────────────────────────────────────────────────────
int PinManager::handleModeAuto(LogEntry &logEntry,
                                int &measuredPower, bool &doML)
{
    // ── Compute available PV surplus ──
    measuredPower = getMeanOfAvailAblePower();
    logEntry.power = measuredPower;

    // ── Compute target power ──
    int targetPower = basePower(measuredPower);

    // ── Cloud-cover down-ramp protection ──
    // If PV surplus dropped sharply (clouds passing), smooth the reduction
    // instead of cutting power all at once
    int diff = targetPower - lastSmoothedPower;

    if (diff < 0)
    {
        // Downward: limit rate to RAMP_DOWN_STEP
        int maxDrop = DEAD_BAND + HYSTERESIS_WATT; // smooth step
        if (abs(diff) > maxDrop)
        {
            targetPower = (int)(lastSmoothedPower - maxDrop);
            if (targetPower < 0)
                targetPower = 0;
        }
    }

    // Smooth the target for logging
    lastSmoothedPower = targetPower;

    // ── TinyNN prediction integration ──
#ifdef TINYNN_ENABLE
    if (tinyNN != nullptr && doML)
    {
        // Normalize inputs for neural net
        float normTemp = ((float)m_sensor1 - m_tempMin)
                         / ((float)(m_tempMaxAllowed - m_tempMin));
        normTemp = fmaxf(0.0f, fminf(1.0f, normTemp));

        // Query TinyNN for preheat recommendation
        TinyNNPrediction pred = tinyNN->predict(
            normTemp,
            m_weatherPvRatio,
            m_weatherCloudAvg,
            m_weatherOutTemp);

        m_tinyNN_preheat = pred.preheat_score;
        m_tinyNN_buffer = pred.buffer_pct;

        // Adjust target if preheat recommended
        if (pred.preheat_score > 0.5f && m_weatherBonus > 0)
        {
            // Aggressive preheat: boost target proportionally
            int boost = (int)(onePhase * pred.preheat_score
                              * pred.buffer_pct);
            targetPower += boost;
            targetPower = basePower(targetPower);
        }

        // Remember experience for training
        float reward = computeMLReward(measuredPower, targetPower,
                                       resolveInputTemp());

        TinyNNExperience exp;
        exp.temp = normTemp * 2.0f - 1.0f;       // [-1, 1]
        exp.pv_ratio = m_weatherPvRatio * 2.0f - 1.0f; // [-1, 1]
        exp.cloud_avg = m_weatherCloudAvg;        // [0, 1]
        exp.temp_outside = m_weatherOutTemp;      // raw °C

        // Target: scale preheat score to [0, 1]
        exp.target_preheat = pred.preheat_score;
        exp.target_buffer = pred.buffer_pct;

        tinyNN->remember(exp);
        tinyNN->trainReplay();
    }
#endif

    // ── Apply target power to hardware ──
    apply(logEntry, targetPower);

    // ── Boiler heating state ──
    m_out_boilerHeating = (currentPhases > 0 || currentPWM > 0);

    // ── Log entry fields ──
    logEntry.state = (uint8_t)MODE_AUTO;
    logEntry.power = targetPower;
    logEntry.pwm = currentPWM;
    logEntry.temp = resolveInputTemp();

    // ── TinyNN log entry ──
#ifdef TINYNN_ENABLE
    // Log stack warning if heap is low
    logStackWarning();
#endif

    return 0;
}

// ──────────────────────────────────────────────────────────────────────────
// computeMLReward — reward signal for TinyNN training
//
// Positive reward: high surplus utilization, temp increasing toward setpoint
// Negative reward: wasting surplus (grid feed-in), temp overshooting
// ──────────────────────────────────────────────────────────────────────────
float PinManager::computeMLReward(int measuredPower, int targetPower, int temp)
{
    float reward = 0.0f;

    // Reward: match target to available surplus (efficient utilization)
    float utilization = (float)targetPower / (float)max(measuredPower, 1);
    utilization = fminf(utilization, 1.0f);
    reward += utilization * 0.5f;

    // Reward: temperature approaching optimal mid-range
    int midTemp = (m_tempMaxAllowed + m_tempMin) / 2;
    int tempDiff = abs(temp - midTemp);
    int maxDiff = max(m_tempMaxAllowed - m_tempMin, 1);
    float tempScore = 1.0f - ((float)tempDiff / (float)maxDiff);
    reward += tempScore * 0.3f;

    // Bonus: if surplus is being fully consumed
    if (measuredPower > 0 && targetPower >= measuredPower * 0.9)
    {
        reward += 0.2f;
    }

    return reward;
}

// ──────────────────────────────────────────────────────────────────────────
// logStackWarning — check ESP free heap and warn if low
// ──────────────────────────────────────────────────────────────────────────
void PinManager::logStackWarning()
{
#ifdef DEBUG_OUTPUT
    // ESP32 free heap check
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) // below 10KB → warning
    {
        LOG_WARNING(TAG_PID, "Low heap memory: %d bytes", freeHeap);
    }
#endif
}

// ──────────────────────────────────────────────────────────────────────────
// UPDATE — main entry point, called from servicePid() every ~1000ms
//
// This method is called with BOTH appLockData() + appLockPidOut() held.
//
// Algorithm:
//   1. pidLockRead() — copy shared data → local members
//   2. preCheck() — determine control mode
//   3. Call appropriate mode handler
//   4. pidLockWrite() — write outputs back to shared state
//   5. Log state change if hardware changed (thread-safe, own mutex)
// ──────────────────────────────────────────────────────────────────────────
void PinManager::update(WEBSOCK_DATA &webSockData,
                        int tempMaxBonusC,
                        float pvRatio,
                        float cloudAvg,
                        float tempOutside)
{
    // ── Copy weather/prediction inputs ──
    m_weatherBonus = tempMaxBonusC;
    m_weatherPvRatio = pvRatio;
    m_weatherCloudAvg = cloudAvg;
    m_weatherOutTemp = tempOutside;

    unsigned long now = millis();

    // ── Step 1: Copy shared data into local members ──
    pidLockRead(webSockData);

    // ── FORCE HEATING: Override alles wenn forceHeating > 0 ──
    // forceHeating values:
    //   1 = HEATING_ON_PHASE_1
    //   2 = HEATING_ON_PHASE_1_2
    //   3 = HEATING_ON_PHASE_1_2_3 (ALLE PHASEN)
    //   0 = HEATING_AUTOMATIC (Auto-Modus)
    if (m_forceHeating > 0) {
        allOn();  // Alle Phase on, unabhängig von PV-Status

        LogEntry logEntry;
        logEntry.ts = now;
        logEntry.state = (uint8_t)MODE_MANUAL;
        logEntry.power = onePhase * 3;
        logEntry.pwm = (uint8_t)OUTPUT_MAX;
        logEntry.temp = resolveInputTemp();

        utils_logWrite(webSockData.logBuffer, logEntry);
        pidLockWrite(webSockData);
        return;  // Skip auto-control, force ist aktiv
    }

    // ── Step 2: Resolve effective temperature ──
    int temp = resolveInputTemp();

    // ── Step 3: Determine control mode ──
    ControlMode mode;
    bool doML;
    preCheck(temp, now, mode, doML);

    // ── Step 4: Handle forced heating from setup ──
    int measuredPower = 0;

    switch (mode)
    {
        case MODE_OFF:
        {
            LogEntry logEntry;
            logEntry.ts = now;
            handleModeOff(logEntry);

            utils_logWrite(webSockData.logBuffer, logEntry);
            pidLockWrite(webSockData);
            break;
        }

        case MODE_LEGIONELLA:
        {
            legionella = true;
            allOn();

            LogEntry logEntry;
            logEntry.ts = now;
            logEntry.state = (uint8_t)MODE_LEGIONELLA;
            logEntry.power = onePhase * 3;
            logEntry.pwm = (uint8_t)OUTPUT_MAX;
            logEntry.temp = temp;
            utils_logWrite(webSockData.logBuffer, logEntry);

            pidLockWrite(webSockData);
            break;
        }

        case MODE_MIN_TEMP:
        {
            allOn();

            LogEntry logEntry;
            logEntry.ts = now;
            logEntry.state = (uint8_t)MODE_MIN_TEMP;
            logEntry.power = onePhase * 3;
            logEntry.pwm = (uint8_t)OUTPUT_MAX;
            logEntry.temp = temp;
            utils_logWrite(webSockData.logBuffer, logEntry);

            pidLockWrite(webSockData);
            break;
        }

        case MODE_AUTO:
        {
            LogEntry logEntry;
            logEntry.ts = now;
            logEntry.state = (uint8_t)MODE_AUTO;
            logEntry.power = 0;
            logEntry.pwm = 0;
            logEntry.temp = temp;

            measuredPower = getMeanOfAvailAblePower();

            if (measuredPower > 0 || doML)
            {
                handleModeAuto(logEntry,
                               measuredPower, doML);
            }
            else
            {
                // No surplus → heater off
                handleModeOff(logEntry);
            }

            utils_logWrite(webSockData.logBuffer, logEntry);
            pidLockWrite(webSockData);
            break;
        }

        default:
        {
            // Fallback: off
            LogEntry logEntry;
            logEntry.ts = now;
            handleModeOff(logEntry);
            utils_logWrite(webSockData.logBuffer, logEntry);
            pidLockWrite(webSockData);
            break;
        }
    }
}
