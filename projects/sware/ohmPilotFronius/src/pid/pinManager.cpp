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
#include "batteryMonitor.h"
#include "logCommitter.h"
// ── Constants (legacy, used by original code) ──
#define FASE_EINS_POWER 576     // W per phase (standard 3-phase heater)
#define PUFFER_AKKU_PRIORI 99   // Battery SoC threshold for priority
#define RAMP_DOWN_STEP 100      // Max W decrease per ramp interval (per phase, legacy)
#define RAMP_DOWN_STEP_TOTAL 300 // Max W decrease TOTAL (3 phases × 100)
#define EMA_ALPHA 0.15          // Glättungsfaktor (0.15 → 160s Halbwertszeit bei 40s Fetch)
// ──────────────────────────────────────────────────────────────────────────
// Constructor
// ──────────────────────────────────────────────────────────────────────────
PinManager::PinManager()
{
    lastLegionella = 0;
    legionella = false;
    m_sensor1 = 0;
    m_sensor2 = 0;
    m_froniusAPI = false;
    m_boilerHeating = false;
    m_tempMaxAllowed = 60;
    m_tempMin = 45;
    m_legionellenMaxTemp = 0;
    m_legionellenDelta = 0;
    m_forceHeating = 0;
    m_forceHeatingStop = 0;
    m_out_boilerHeating = false;
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
void PinManager::config(WEBSOCK_DATA& data, int l1, int l2, int pwm)
{
    // Initialize PhaseController with pin numbers
    phaseCtrl.initPins(l1, l2, pwm);
    // TinyNN needs onePhase for target bounds
    float totalPower = (float)data.setupData.heizstab_leistung_in_watt;
    int onePhase = (totalPower > 0.0f) ? (int)(totalPower / 3.0f) : FASE_EINS_POWER;
    phaseCtrl.setOnePhase(onePhase);
    // Initialize MLPredictor with temp bounds and max power
    int maxPower = onePhase * 3;
    mlPredictor.init(
        (float)data.setupData.tempMinInGrad,
        (float)data.setupData.tempMaxAllowedInGrad,
        maxPower);
#ifdef TINYNN_ENABLE
    // Initialize TinyNN if enabled
    if (tinyNN == nullptr)
    {
        tinyNN = new TinyNN((float)maxPower,
            (float)data.setupData.tempMinInGrad,
            (float)data.setupData.tempMaxAllowedInGrad);
        tinyNN->load();
    }
    // Pass TinyNN pointer to MlPredictor
    mlPredictor.setTinyNN(tinyNN);
#endif
    LOG_INFO(TAG_PID,
        "PinManager configured: relay1=%d relay2=%d pwm=%d onePhase=%dW maxTemp=%d minTemp=%d",
        l1, l2, pwm, onePhase,
        data.setupData.tempMaxAllowedInGrad,
        data.setupData.tempMinInGrad);
}
// ──────────────────────────────────────────────────────────────────────────
// testPins — hardware verification blip
// ──────────────────────────────────────────────────────────────────────────
void PinManager::testPins(int, int, int)
{
    phaseCtrl.testPins();
}
// ──────────────────────────────────────────────────────────────────────────
// reset — clear all state, turn off pins, clear targets
// ──────────────────────────────────────────────────────────────────────────
void PinManager::reset()
{
    LOG_INFO(TAG_PID, "PinManager reset() called. Heater off.");
    phaseCtrl.allOff();
    lastLegionella = 0;
    legionella = false;
    lastSmoothedPower = 0;
    m_froniusAPI = false;
    m_boilerHeating = false;
    m_out_boilerHeating = false;
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
    // pwmPin is private in PhaseController, accessed via friend
    if (phaseCtrl.pwmPin > 0)
        return analogRead(phaseCtrl.pwmPin);
    return 0;
}
// ──────────────────────────────────────────────────────────────────────────
// pidLockRead — copy shared data into local members (thread-safe under lock)
//
// Called from update() while BOTH data + pidOut locks are held.
// Copies PV/grid/temperature data from WEBSOCK_DATA → private members.
// ──────────────────────────────────────────────────────────────────────────
void PinManager::pidLockRead(WEBSOCK_DATA& webSockData)
{
    m_sensor1 = webSockData.temperature.sensor1;
    m_sensor2 = webSockData.temperature.sensor2;
    m_froniusAPI = webSockData.states.froniusAPI;
    m_boilerHeating = webSockData.states.boilerHeating;
    m_tempMaxAllowed = webSockData.setupData.tempMaxAllowedInGrad;
    m_tempMin = webSockData.setupData.tempMinInGrad;
    m_legionellenMaxTemp = webSockData.setupData.legionellenMaxTemp;
    m_legionellenDelta = webSockData.setupData.legionellenDelta;
    m_forceHeating = webSockData.setupData.forceHeating;
    m_forceHeatingStop = (int)webSockData.setupData.forceHeatingStop;
    // ── Energy state → BatteryMonitor ──
    if (m_froniusAPI)
    {
#ifdef FRONIUS_IV
        batteryMon.update(
            webSockData.fronius_SOLAR_POWERFLOW.p_grid,
            webSockData.fronius_SOLAR_POWERFLOW.p_akku,
            webSockData.fronius_SOLAR_POWERFLOW.soc,
            webSockData.setupData.akkuPriori,
            webSockData.setupData.wattSetupForTest);
#endif
    }
#ifdef AMIS_READER_DEV
    else
    {
        // AmisReader Smart-Meter am Netzgrenzpunkt:
        //   saldo  = 1.7.0 - 2.7.0  ≡  p_grid  (+Bezug, −Einspeisung)
        batteryMon.update(
            (float)webSockData.amisReader.saldo,
            0,  // No battery power data
            0,  // No SOC data
            webSockData.setupData.akkuPriori,
            webSockData.setupData.wattSetupForTest);
    }
#endif
}
// ──────────────────────────────────────────────────────────────────────────
// pidLockWrite — write local output back to shared pidContainer
//
// Called from update() while BOTH data + pidOut locks are held.
// Updates pidContainer (read by webSocket JSON builder) and states.
// ──────────────────────────────────────────────────────────────────────────
void PinManager::pidLockWrite(WEBSOCK_DATA& webSockData)
{
    // PhaseController provides current phase/PWM state
    int cp = phaseCtrl.getActivePhases();
    int cw = phaseCtrl.getPwm();
    webSockData.pidContainer.mCurrentPower = phaseCtrl.getHeaterPower();
    webSockData.pidContainer.PID_PIN1 = (cp >= 1);
    webSockData.pidContainer.PID_PIN2 = (cp >= 2);
    webSockData.pidContainer.mAnalogOut = cw;
    // Weather + TinyNN prediction fields
    webSockData.pidContainer.weatherBonus = m_weatherBonus;
    webSockData.pidContainer.weatherPvRatio = m_weatherPvRatio;
    webSockData.pidContainer.weatherCloudAvg = m_weatherCloudAvg;
    webSockData.pidContainer.weatherOutTemp = m_weatherOutTemp;
    // Boiler heating status
    m_out_boilerHeating = (cp > 0 || cw > 0);
    webSockData.states.boilerHeating = m_out_boilerHeating;
#ifdef TINYNN_ENABLE
    webSockData.pidContainer.tinyNN_preheat_score = m_tinyNN_preheat;
    webSockData.pidContainer.tinyNN_buffer_pct = m_tinyNN_buffer;
#endif
}
// ──────────────────────────────────────────────────────────────────────────
// resolveInputTemp — delegated to ThermalController
// ──────────────────────────────────────────────────────────────────────────
int PinManager::resolveInputTemp() const
{
    return ThermalController::resolveInputTemp(
        m_sensor1, m_sensor2, m_tempMaxAllowed, m_tempMin);
}
// ──────────────────────────────────────────────────────────────────────────
// preCheck — delegated to ThermalController
// ──────────────────────────────────────────────────────────────────────────
void PinManager::preCheck(int temp, unsigned long nowMS,
    ControlMode& mode, bool& doML)
{
    int effectiveMax = (int)(m_tempMaxAllowed + m_weatherBonus);
    ThermalController::evaluateMode(
        temp, effectiveMax, m_tempMin,
        m_legionellenMaxTemp, m_legionellenDelta,
        nowMS,
        legionella, lastLegionella,
        mode, doML);
}
// ──────────────────────────────────────────────────────────────────────────
// handleModeOff — max-temp reached; heater off, log state
// ──────────────────────────────────────────────────────────────────────────
void PinManager::handleModeOff(LogEntry& logEntry)
{
    allOff();
    logEntry.state = (uint8_t)MODE_OFF;
    logEntry.power = 0;
    logEntry.pwm = 0;
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
int PinManager::handleModeAuto(LogEntry& logEntry,
    int& measuredPower, bool& doML)
{
    // ── Compute available PV surplus ──
    measuredPower = batteryMon.getAvailablePower(phaseCtrl.getHeaterPower(), m_out_wattBiasForTest);
    logEntry.power = measuredPower;
    // ── Compute target power ──
    int maxPower = phaseCtrl.getOnePhase() * 3;
    int targetPower = std::max(0, std::min(measuredPower, maxPower));
    // ── Clamp down-ramp for cloud-cover protection ──
    targetPower = mlPredictor.clampRampDown(targetPower, lastSmoothedPower);
    // ── TinyNN ML adjustment (returns adjusted target) ──
    int tinyNN_preheat = 0, tinyNN_buffer = 0;
    targetPower = mlPredictor.adjustTarget(
        targetPower, measuredPower, doML,
        m_sensor1,
        m_weatherPvRatio, m_weatherCloudAvg, m_weatherOutTemp,
        m_weatherBonus, phaseCtrl.getOnePhase(),
        tinyNN_preheat, tinyNN_buffer);
    m_tinyNN_preheat = tinyNN_preheat;
    m_tinyNN_buffer = tinyNN_buffer;
    // ── Smoothed logging state ──
    lastSmoothedPower = targetPower;
    // ── Apply target power to hardware ──
    phaseCtrl.apply(targetPower, logEntry);
    // ── Boiler heating state ──
    m_out_boilerHeating = (phaseCtrl.getActivePhases() > 0 || phaseCtrl.getPwm() > 0);
    // ── Log entry fields ──
    logEntry.state = (uint8_t)MODE_AUTO;
    logEntry.power = targetPower;
    logEntry.pwm = phaseCtrl.getPwm();
    logEntry.temp = resolveInputTemp();
    return 0;
}
// ──────────────────────────────────────────────────────────────────────────
//
void PinManager::update(WEBSOCK_DATA& webSockData,
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
    // ── Step 2: Resolve effective temperature (VOR Force-Checks!) ──
    int temp = resolveInputTemp();
    int effectiveMax = m_tempMaxAllowed + m_weatherBonus;
    // ── FORCE-OFF: Heater Kill (aber Frostschutz gewinnt!) ──
    if (m_forceHeatingStop > 0) {
        // Frostschutz: unter min-Temp NICHT abschalten
        if (temp < m_tempMin) {
            LOG_INFO(TAG_PID, "Force-Stop aufgehoben: Frostschutz (temp=%d < min=%d)", temp, m_tempMin);
            // Fallt auf normale Steuerung durch
        } else {
            LogEntry logEntry;
            logEntry.ts = now;
            logEntry.temp = temp;
            handleModeOff(logEntry);
            LogCommitter::writeToLog(logEntry, webSockData);
            pidLockWrite(webSockData);
            return;
        }
    }
    // ── FORCE-HEAT: Vollgas aber mit Temperaturschutz ──
    if (m_forceHeating > 0) {
        // Temperaturschutz: über max-Temp sofort ab
        if (temp >= effectiveMax) {
            LOG_INFO(TAG_PID, "Force-Heat unterbrochen: Max-Temp erreicht (temp=%d >= max=%d)", temp, effectiveMax);
            LogEntry logEntry;
            logEntry.ts = now;
            logEntry.temp = temp;
            handleModeOff(logEntry);
            LogCommitter::writeToLog(logEntry, webSockData);
            pidLockWrite(webSockData);
            return;
        }
        allOn();
        LogCommitter::commit(now, (uint8_t)MODE_MANUAL, phaseCtrl.getOnePhase() * 3, (uint8_t)OUTPUT_MAX, temp, webSockData);
        pidLockWrite(webSockData);
        return;
    }
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
        logEntry.temp = temp;
        handleModeOff(logEntry);
        LogCommitter::writeToLog(logEntry, webSockData);
        pidLockWrite(webSockData);
        break;
    }
    case MODE_LEGIONELLA:
    {
        legionella = true;
        allOn();
        LogCommitter::commit(now, (uint8_t)MODE_LEGIONELLA, phaseCtrl.getOnePhase() * 3, (uint8_t)OUTPUT_MAX, temp, webSockData);
        pidLockWrite(webSockData);
        break;
    }
    case MODE_MIN_TEMP:
    {
        allOn();
        LogCommitter::commit(now, (uint8_t)MODE_MIN_TEMP, phaseCtrl.getOnePhase() * 3, (uint8_t)OUTPUT_MAX, temp, webSockData);
        pidLockWrite(webSockData);
        break;
    }
    case MODE_AUTO:
    {
        LogEntry logEntry;
        LogCommitter::fillEntry(logEntry, now, (uint8_t)MODE_AUTO, 0, 0, temp);
        measuredPower = batteryMon.getAvailablePower(phaseCtrl.getHeaterPower(), m_out_wattBiasForTest);
        if (measuredPower > 0)
        {
            handleModeAuto(logEntry, measuredPower, doML);
        }
        else
        {
            handleModeOff(logEntry);
        }
        LogCommitter::writeToLog(logEntry, webSockData);
        pidLockWrite(webSockData);
        break;
    }
    default:
    {
        LogEntry logEntry;
        logEntry.ts = now;
        logEntry.temp = temp;
        handleModeOff(logEntry);
        LogCommitter::writeToLog(logEntry, webSockData);
        pidLockWrite(webSockData);
        break;
    }
    }
}
