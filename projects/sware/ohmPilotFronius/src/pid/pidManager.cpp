#include "pinManager.h"
#include "app_state.h"
#include "utils.h"
#include "ledHandler.h"
#include "app_sync.h"
#ifdef WEATHER_API
#include "weather.h"
#endif

namespace
{
int clampInt(int value, int minValue, int maxValue)
{
    if (value < minValue)
        return minValue;
    if (value > maxValue)
        return maxValue;
    return value;
}

int wattToPwm(int watt, int onePhase)
{
    if (watt <= 0)
        return 0;
    if (watt >= onePhase)
        return OUTPUT_MAX;

    return (int)(((long)watt * OUTPUT_MAX) / onePhase);
}
}

PinManager::~PinManager()
{
    delete tinyNN;
    tinyNN = nullptr;
}

void PinManager::config(WEBSOCK_DATA &data, int l1, int l2, int pwm)
{
    if (tinyNN != nullptr)
    {
        LOG_DEBUG(TAG_PID, "PinManager::config - TinyNN wird freigegeben vor Neukonfiguration");
        delete tinyNN;
        tinyNN = nullptr;
    }

    onePhase = data.setupData.heizstab_leistung_in_watt / 3;

    LOG_INFO(TAG_PID, "PinManager::config - Heizpatrone Leistung %d Watt",
             data.setupData.heizstab_leistung_in_watt);
    pinL1 = l1;
    pinL2 = l2;
    pwmPin = pwm;
    epsilon = data.setupData.epsilonML_PinManager;
    pinMode(pinL1, OUTPUT);
    pinMode(pinL2, OUTPUT);
    pinMode(pwmPin, OUTPUT);
    legionella = false;
    availablePower.clear();
    reset();
    int delta = data.setupData.tempMaxAllowedInGrad - data.setupData.tempMinInGrad;

    tinyNN = new TinyNN(data.setupData.heizstab_leistung_in_watt, delta / 2.0, delta);
}

/*
 *************************************************************************
 * ── Thread-safe Lock/Unlock helpers ─────────────────────────────
 * pidLockRead()  : Copy input fields from WEBSOCK_DATA into local m_*
 *                  Caller must hold dataMutex + pidOutMutex.
 * pidLockWrite() : Write output fields (wattBias, boilerHeating, pidContainer)
 *                  Caller must hold dataMutex + pidOutMutex.
 * utils_logWrite() is deliberately OUTSIDE the lock to prevent
 * an ABBA-deadlock (R01) with its own rb.mutex.
 *************************************************************************
 */

void PinManager::pidLockRead(WEBSOCK_DATA &data)
{
    m_sensor1              = data.temperature.sensor1;
    m_sensor2              = data.temperature.sensor2;
    m_boilerHeating        = data.setupData.forceHeating;
    m_tempMaxAllowed       = (int)data.setupData.tempMaxAllowedInGrad;
    m_tempMin              = (int)data.setupData.tempMinInGrad;
    m_legionellenMaxTemp   = (int)data.setupData.legionellenMaxTemp;
    m_legionellenDelta     = data.setupData.legionellenDelta;
    m_akkuPriori           = data.setupData.akkuPriori;
    m_akkuLadung           = data.mbContainer.akkuStr.data.chargeRate;
    m_gridPower            = data.mbContainer.inverterSumValues.data.acCurrentPower;
    m_meterPower           = data.mbContainer.meterValues.data.acCurrentPower;
    m_wattSetupForTest     = data.setupData.wattSetupForTest;
}

void PinManager::pidLockWrite(WEBSOCK_DATA &data)
{
    data.states.boilerHeating      = m_out_boilerHeating;
    data.states.wattBiasForTest    = m_out_wattBiasForTest;
    data.pidContainer.mAnalogOut   = currentPWM;
    data.pidContainer.PID_PIN1     = digitalRead(pinL1) == HIGH ? 1 : 0;
    data.pidContainer.PID_PIN2     = digitalRead(pinL2) == HIGH ? 1 : 0;
}

/*
****** STATE
*/

int PinManager::tempState(double t)
{
    if (t < 45) return 0;
    if (t < 50) return 1;
    if (t < 55) return 2;
    if (t < 60) return 3;
    return 4;
}

int PinManager::pvState(double p)
{
    p = -p;
    if (p < 500)  return 0;
    if (p < 1500) return 1;
    if (p < 3000) return 2;
    if (p < 5000) return 3;
    return 4;
}

/*
****** Base Power
*/
int PinManager::basePower(int effective)
{
    int phases = (int)(effective / onePhase);
    return phases > 2 ? 2 : phases;
}

/*
****** testPins – GPIO pin test sequence
*/
void PinManager::testPins(int l1, int l2, int pwm)
{
    LOG_INFO("GPIO", "testPins BEGIN");
    LOG_INFO("GPIO", "Port l1 ist jetzt HIGH");
    digitalWrite(l1, HIGH);
    vTaskDelay(pdMS_TO_TICKS(4000));
    digitalWrite(l1, LOW);

    LOG_INFO("GPIO", "Port l2 ist jetzt HIGH");
    digitalWrite(l2, HIGH);
    vTaskDelay(pdMS_TO_TICKS(4000));
    digitalWrite(l2, LOW);

    LOG_INFO("GPIO", "Port pwm ist jetzt HIGH");
    analogWrite(pwm, 255);
    vTaskDelay(pdMS_TO_TICKS(4000));
    analogWrite(pwm, 0);
    LOG_INFO("GPIO", "testPins EXIT");
}

/*
****** preCheck – guard logic: early-out on max/min temp, legionella, manual
*/
void PinManager::preCheck(int temp, unsigned long nowMS, ControlMode &mode,
                          bool &doML)
{
    LOG_DEBUG("PinManager::PID: : %s", pcTaskGetName(NULL));

    // Max-Temp: sofortigen Stop ausloesen
    if (temp >= m_tempMaxAllowed)
    {
        LOG_DEBUG(TAG_PID, "PID: Max Temp <%d> erreicht: <%d>", m_tempMaxAllowed, temp);
        reset();
        mode   = MODE_OFF;
        doML   = false;
        return;
    }

    // Legionella
    if (nowMS - lastLegionella > m_legionellenDelta)
        legionella = true;

    if (legionella)
    {
        if (temp >= m_legionellenMaxTemp)
        {
            legionella       = false;
            lastLegionella   = nowMS;
            LOG_DEBUG(TAG_PID, "PID: Legionella Temp erreicht, AUS");
            mode   = MODE_OFF;
            doML   = false;
            return;
        }
        else
        {
            LOG_DEBUG(TAG_PID, "PID: HEAT (Legionella)");
            mode   = MODE_LEGIONELLA;
            doML   = false;
            return;
        }
    }

    // Min-Temp: Frostschutz
    if (temp < m_tempMin)
    {
        LOG_DEBUG(TAG_PID, "PID: Min Temp <%d> erreicht: <%d>", m_tempMin, temp);
        powerIndex = 0;
        mode       = MODE_MIN_TEMP;
        doML       = false;
        return;
    }

    // Manuelle Steuerung
    if (m_boilerHeating != HEATING_AUTOMATIC)
    {
        LOG_DEBUG(TAG_PID, "PID: Manuelle Steuerung");
        powerIndex = 0;
        mode       = MODE_MANUAL;
        doML       = false;
        return;
    }

    // Kein Guard-Zweig getreten → AUTO, ML ok
    mode = MODE_AUTO;
    doML = true;
}

/*
****** resolveInputTemp – compute fallback-safe average temperature
*/
int PinManager::resolveInputTemp() const
{
    int temp = (m_sensor1 + m_sensor2) / 2;
    if (m_sensor1 < 0) temp = m_sensor2;
    if (m_sensor2 < 0) temp = m_sensor1;
    return temp;
}

/*
****** handleModeOff – MODE_OFF: reset hardware, clear logEntry
*/
void PinManager::handleModeOff(LogEntry &logEntry)
{
    reset();
    logEntry.state = 0;
    logEntry.power = 0;
    logEntry.pwm   = 0;
    LOG_INFO(TAG_PID, "HEAT OFF – alle Aus");
}

/*
****** handleForceHeating – MODE_LEGIONELLA + MODE_MIN_TEMP: full power
*/
void PinManager::handleForceHeating(LogEntry &logEntry, int &measuredPower)
{
    int fullPower = onePhase * 3;
    measuredPower = fullPower;
    currentPWM    = 255;
    logEntry.state = 3;
    logEntry.power = fullPower;
    logEntry.pwm   = 255;
    LOG_INFO(TAG_PID, "HEAT ON (Voll) – Relais + PWM max");
}

/*
****** handleModeManual – MODE_MANUAL: override everything
*/
void PinManager::handleModeManual()
{
    LOG_INFO(TAG_PID, "Manuelle Steuerung");
    availablePower.clear();
}

/*
****** handleModeAuto – MODE_AUTO: compute measured/target power, decide ML
**  return value = targetPower in Watt
*/
int PinManager::handleModeAuto(LogEntry &logEntry, int &measuredPower, bool &doML)
{
    measuredPower    = getMeanOfAvailAblePower();
    logEntry.power   = measuredPower;
    LOG_DEBUG(TAG_PID, "AUTO – gemessen: %d W", measuredPower);

    // keine nutzbare PV-Leistung → Ausschalten
    if (abs(measuredPower) < EPSILON_TEMP || measuredPower > 0)
    {
        LOG_INFO(TAG_PID, "AUTO – kein Überschuss: %d W", measuredPower);
        doML = false;
        return 0;
    }

    // nutzbare PV-Leistung vorhanden
    int heater             = heaterPower();
    int effectiveAvailable = (-measuredPower) + heater;
    LOG_DEBUG(TAG_PID, "AUTO – effektiv: %d W", effectiveAvailable);

    int target = clampInt(effectiveAvailable, 0, onePhase * 3);
    if (target > effectiveAvailable + 50)
    {
        target = effectiveAvailable;
    }

    doML = true;
    return target;
}

/*
****** computeMLReward – RL-Belohnung für den aktuellen Zustand
**  Skalierte Strafe, Sweet-Spot-Bonus, Temperatur-Management,
**  Hardware-Schonung und Sicherheit
*/
float PinManager::computeMLReward(int measuredPower, int targetPower, int temp)
{
    float reward = 0.0f;

    // 1. "Sweet Spot" (Netz-Null-Punkt)
    if (measuredPower > 0)
    {
        reward -= (measuredPower / 50.0f);  // Bezug: Strafe
    }
    else if (measuredPower < 0)
    {
        if (measuredPower > -50)
            reward += 10.0f;  // Bonus wenn nah an der Null
        else
            reward += 2.0f;   // Kleiner Bonus
    }

    // 2. Temperatur-Management (Ziel: 57°C)
    float tempDiff = abs(temp - 57);
    if (tempDiff < 2)
        reward += 5.0f;
    else
        reward -= (tempDiff * 0.5f);

    // 3. Hardware-Schonung (Relais-Check)
    static int lastActionPhases = 0;
    int currentActionPhases = (int)(targetPower / onePhase);
    if (currentActionPhases != lastActionPhases)
        reward -= 5.0f;
    lastActionPhases = currentActionPhases;

    // 4. Sicherheit (Extremwerte)
    if (temp > (m_tempMaxAllowed - 2))
        reward -= 20.0f;

    return reward;
}

/*
****** logStackWarning – check Stack High-Water Mark
*/
void PinManager::logStackWarning()
{
    UBaseType_t stackLeft = uxTaskGetStackHighWaterMark(NULL);
    if (stackLeft < 200)
    {
        LOG_ERROR(TAG_PID, "STACK FAST VOLL! %u Wörter frei", stackLeft);
    }
}

/*
****** Main UPDATE
**  Orchestrierung: Input → Guard → Mode → Apply → Write → ML
*/
void PinManager::update(WEBSOCK_DATA &webSockData, int tempMaxBonusC)
{
    unsigned long now = millis();
    LogEntry logEntry{};
    time_t curT;
    time(&curT);

    // ── STEP 1: thread-safe read into local members ──
    pidLockRead(webSockData);
    m_out_boilerHeating = false;

    // ── Wetter-Bonus: tempMaxAllowed dynamisch anpassen ──
    //    bonus>0 → boiler heizt bis setupMax + bonus (capped bei MAX_SAFE)
#ifdef WEATHER_API
    if (tempMaxBonusC > 0)
    {
        int effectiveMax = m_tempMaxAllowed + tempMaxBonusC;
        if (effectiveMax > WEATHER_PREHEAT_MAX_SAFE)
            effectiveMax = WEATHER_PREHEAT_MAX_SAFE;
        LOG_INFO(TAG_PID, "WEATHER: +%d°C → maxTemp %d→%d°C",
                 tempMaxBonusC, m_tempMaxAllowed, effectiveMax);
        m_tempMaxAllowed = effectiveMax;
    }
#else
    (void)tempMaxBonusC;
#endif

    // Temperatur validieren (früh-out bei ungültigen Sensoren)
    int temp = resolveInputTemp();
    if (temp <= 0)
    {
        LOG_ERROR(TAG_PID, "Ungültig: s1=%d s2=%d", m_sensor1, m_sensor2);
        logEntry.temp = 0;
        logEntry.ts   = curT;
        reset();
        pidLockWrite(webSockData);
        utils_logWrite(webSockData.logBuffer, logEntry);
        return;
    }

    logEntry.temp = temp;
    logEntry.ts   = curT;
    m_froniusAPI  = webSockData.states.froniusAPI;

    // ── STEP 2: Guard-Checks (Legionella, Min/Max Temp, Manual) → MODE ──
    ControlMode currentMode;
    bool doML = true;
    preCheck(temp, now, currentMode, doML);

    int measuredPower = 0;
    int targetPower   = 0;
    int action        = 0;

    LOG_INFO(TAG_PID, "PID update – Mode: %d, temp: %d, s1: %d, s2: %d",
             currentMode, temp, m_sensor1, m_sensor2);

    switch (currentMode)
    {
    case MODE_OFF:
        handleModeOff(logEntry);
        doML = false;
        break;

    case MODE_LEGIONELLA:
    case MODE_MIN_TEMP:
        handleForceHeating(logEntry, measuredPower);
        targetPower = measuredPower;
        doML   = false;
        break;

    case MODE_MANUAL:
        handleModeManual();
        targetPower = onePhase * 3;
        doML   = false;
        break;

    case MODE_AUTO:
        targetPower = handleModeAuto(logEntry, measuredPower, doML);
        action      = targetPower / onePhase;
        break;

    default:
        LOG_ERROR(TAG_PID, "Unerkannter Modus: %d", currentMode);
        doML = false;
        break;
    }

    // ── STEP 3: Hardware-Ansteuerung ──
    LOG_INFO(TAG_PID, "PID apply – Ziel: %d W", targetPower);
    if (targetPower > 0)
    {
        m_out_boilerHeating = true;
    }
    apply(logEntry, targetPower);

    vTaskDelay(pdMS_TO_TICKS(50));  // Atempause

    // ── STEP 4: Stack-Check (Diagnose) ──
    logStackWarning();

    // ── STEP 5: thread-safe write outputs + log outside lock ──
    pidLockWrite(webSockData);
    utils_logWrite(webSockData.logBuffer, logEntry);

    // ── STEP 6: ML-Training (nur bei MODE_AUTO) ──
    if (doML)
    {
        LOG_INFO(TAG_PID, "ML train – Task: %s", pcTaskGetTaskName(NULL));
        float reward = computeMLReward(measuredPower, targetPower, temp);
        tinyNN->remember(temp, measuredPower, action, (int)reward);
        tinyNN->trainReplay();
    }
}

/*
****** aPPLY
*/

void PinManager::apply(LogEntry &logEntry, int targetPower)
{
    LOG_INFO(TAG_PID, "PinManager::apply: Task %s, Ziel: %d W",
             pcTaskGetName(NULL), targetPower);
    unsigned long now = millis();
    targetPower = clampInt(targetPower, 0, onePhase * 3);

    // ── HARD STOP ────────────────────────────────────────────────────────
    if (targetPower < 50)
    {
        digitalWrite(pinL1, LOW);
        digitalWrite(pinL2, LOW);
        analogWrite(pwmPin, 0);
        currentPWM = 0;

        logEntry.state = 0;
        logEntry.pwm   = 0;
        logEntry.power = 0;
        return;
    }

    // ── Relay-Hysterese (asymmetrisch: Einschalten vs Ausschalten) ───────
    // marginUp   =  20% der Phasenleistung (Einschalten)
    // marginDown =  40% der Phasenleistung (Ausschalten → Schutz)
    const int marginUp   = onePhase / 5;   // z.B. 300W bei 1500W/Phase
    const int marginDown = onePhase / 2;   // z.B. 600W bei 1500W/Phase

    int currentActive = (digitalRead(pinL1) == HIGH ? 1 : 0)
                      + (digitalRead(pinL2) == HIGH ? 1 : 0);
    int desiredPhases = currentActive;

    // Einschalten → konservativ (erfordert headroom)
    if (desiredPhases < 1 && targetPower >= (onePhase + marginUp))
        desiredPhases = 1;
    else if (desiredPhases < 2 && targetPower >= ((2 * onePhase) + marginUp))
        desiredPhases = 2;

    // Ausschalten → größerer Hysteresedeadband
    else if (desiredPhases >= 1 && targetPower <= (onePhase - marginDown))
        desiredPhases = 0;
    else if (desiredPhases >= 2 && targetPower <= ((2 * onePhase) - marginDown))
        desiredPhases = 1;

    LOG_INFO(TAG_PID, "apply – Ziel: %d W, desired: %d, current: %d",
             targetPower, desiredPhases, currentActive);

    // ── Relais schalten (Cooldown-Schutz) ────────────────────────────────
    if (desiredPhases != currentActive && (now - lastSwitch > MIN_SWITCH))
    {
        digitalWrite(pinL1, desiredPhases >= 1 ? HIGH : LOW);
        digitalWrite(pinL2, desiredPhases >= 2 ? HIGH : LOW);
        lastSwitch    = now;
        currentActive = desiredPhases;
    }

    // ── PWM feinteilen (Soft-Clamp: max 10 Steps/Cycle) ──────────────────
    int powerFromRelays = currentActive * onePhase;
    int powerForPWM     = clampInt(targetPower - powerFromRelays, 0, onePhase);
    int desiredPWM      = wattToPwm(powerForPWM, onePhase);

    // Soft-Clamp: PWM gleitet statt springt
    int pwmDelta = desiredPWM - currentPWM;
    int maxStep  = 10; // max PWM-Änderung pro Zyklus (~2s)
    int step     = abs(pwmDelta) > maxStep ? (pwmDelta > 0 ? maxStep : -maxStep) : pwmDelta;

    currentPWM = clampInt(currentPWM + step, 0, OUTPUT_MAX);
    analogWrite(pwmPin, currentPWM);

    // ── Logging ──────────────────────────────────────────────────────────
    logEntry.pwm   = currentPWM;
    logEntry.power = powerFromRelays + wattToPwm(currentPWM, onePhase);
    int state = 0;
    if (digitalRead(pinL1) == HIGH) state |= 1;
    if (digitalRead(pinL2) == HIGH) state |= 2;
    logEntry.state = state;

    LOG_INFO(TAG_PID, "R1: %d, R2: %d, PWM: %d, W: %d, state: %d",
             currentActive >= 1, currentActive >= 2, currentPWM,
             powerForPWM, state);
}

/*
******* HELPER
*/
int PinManager::heaterPower()
{
    int p = 0;

    if (digitalRead(pinL1)) p += onePhase;
    if (digitalRead(pinL2)) p += onePhase;

    p += (int)(((long)currentPWM * onePhase) / OUTPUT_MAX);

    return p;
}

void PinManager::reset()
{
    digitalWrite(pinL1, LOW);
    digitalWrite(pinL2, LOW);
    analogWrite(pwmPin, 0);
    currentPWM = 0;
}
int PinManager::getStateOfDigPin(short pin)
{
    if (pin == 0) return digitalRead(pinL1) ? 1 : 0;
    if (pin == 1) return digitalRead(pinL2) ? 1 : 0;
    return -1;
}

int PinManager::getStateOfAnaPin()
{
    return (int)currentPWM;
}

void PinManager::allOn()
{
    digitalWrite(pinL1, HIGH);
    digitalWrite(pinL2, HIGH);
    analogWrite(pwmPin, (int)OUTPUT_MAX);

    currentPhases = 2;
    currentPWM    = OUTPUT_MAX;
}
inline int PinManager::getMeanOfAvailAblePower()
{
    // Truncate buffer to prevent unbounded growth
    if (availablePower.size() > HYSTERESIS_WATT)
    {
        availablePower.erase(availablePower.begin());
    }

    size_t n = availablePower.size();
    if (n == 0) return 0;

    // Median-of-centre – no heap, no full-sort
    // Trim 1 lowest + 1 highest, then average the rest
    std::nth_element(availablePower.begin(), availablePower.begin() + 1, availablePower.end());     // 1st trim
    std::nth_element(availablePower.begin() + 1, availablePower.end() - 1, availablePower.end()); // 2nd trim

    double sum = 0;
    for (size_t i = 1; i < n - 1; i++)
        sum += availablePower[i];

    return (int)(sum / (n - 2));
}
