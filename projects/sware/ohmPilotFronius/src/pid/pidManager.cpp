#include "pinManager.h"
#include "app_state.h"
#include "utils.h"
#include "ledHandler.h"
#include "app_sync.h"  // g_appMutex Timeout

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
    // Vermeide Speicherleck bei wiederholtem Aufruf:
    if (tinyNN != nullptr)
    {
        LOG_DEBUG(TAG_PID, "PinManager::config - TinyNN wird freigegeben vor Neukonfiguration");
        delete tinyNN;
        tinyNN = nullptr;
    }

    onePhase = data.setupData.heizstab_leistung_in_watt / 3;

    LOG_INFO(TAG_PID, "PinManager::config:: - Heizpatrone Leistung %d Watt,", data.setupData.heizstab_leistung_in_watt);
    pinL1 = l1;
    pinL2 = l2;
    pwmPin = pwm;
    epsilon = data.setupData.epsilonML_PinManager;
    // memcpy(&data.logBuffer, 0, sizeof(RingBuffer)); // reset log buffer
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
 * ── Thread-safe Lock/Unlock helpers ─────────────────────────────
 * pidLockRead()  : Under appLock(), copy all input fields from
 *                  WEBSOCK_DATA into local m_* members.
 * pidLockWrite() : Under appLock(), write all output fields (output
 *                  bias, boilerHeating state, pidContainer) back.
 * utils_logWrite() is deliberately OUTSIDE the lock to prevent
 * an ABBA-deadlock (R01) with its own rb.mutex.
 ************************************************************************/

void PinManager::pidLockRead(WEBSOCK_DATA &data)
{
    if (!appLock(150)) // R02: fail-fast, do not block forever
    {
        LOG_ERROR(TAG_PID, "pidLockRead: appLock failed! Using stale local copy.");
        return;
    }

    // ── input fields ──
    // Harmonisierte Werte – egal ob Fronius oder AMIS-Reader
    m_sensor1               = data.temperature.sensor1;
    m_sensor2               = data.temperature.sensor2;
    m_boilerHeating         = data.setupData.forceHeating;
    m_tempMaxAllowed        = (int)data.setupData.tempMaxAllowedInGrad;
    m_tempMin               = (int)data.setupData.tempMinInGrad;
    m_legionellenMaxTemp    = (int)data.setupData.legionellenMaxTemp;
    m_legionellenDelta      = data.setupData.legionellenDelta;
    m_akkuPriori            = data.setupData.akkuPriori;
    m_akkuLadung            = data.mbContainer.akkuStr.data.chargeRate;
    m_gridPower             = data.mbContainer.inverterSumValues.data.acCurrentPower;
    m_meterPower            = data.mbContainer.meterValues.data.acCurrentPower;
    m_wattSetupForTest      = data.setupData.wattSetupForTest;

    appUnlock();
}

void PinManager::pidLockWrite(WEBSOCK_DATA &data)
{
    if (!appLock(50))
    {
        LOG_ERROR(TAG_PID, "pidLockWrite: appLock failed! Output values lost.");
        return;
    }

    // ── output fields ──
    data.states.boilerHeating             = m_out_boilerHeating;
    data.states.wattBiasForTest             = m_out_wattBiasForTest;
    data.pidContainer.mAnalogOut          = currentPWM;
    data.pidContainer.PID_PIN1           = digitalRead(pinL1) == HIGH ? 1 : 0;
    data.pidContainer.PID_PIN2           = digitalRead(pinL2) == HIGH ? 1 : 0;

    appUnlock();
}

/*
*********** STATE
*/

int PinManager::tempState(double t)
{
    if (t < 45)
        return 0;
    if (t < 50)
        return 1;
    if (t < 55)
        return 2;
    if (t < 60)
        return 3;
    return 4;
}

int PinManager::pvState(double p)
{
    p = -p;
    if (p < 500)
        return 0;
    if (p < 1500)
        return 1;
    if (p < 3000)
        return 2;
    if (p < 5000)
        return 3;
    return 4;
}

/*
RL

*/

/*
****** Base Power
*/
int PinManager::basePower(int effective)
{
    int phases = (int)(effective / onePhase);
    if (phases > 2)
        phases = 2;

    return phases * onePhase;
}

/*

Main UPDATE
update(double measuredPower, double temp, int hour)
*/
void PinManager::testPins(int l1, int l2, int pwm)
{
    LOG_INFO("GPIO", "testPins, BEGIN");
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
    LOG_INFO("GPIO", "testPins ExITT");
}

inline ControlMode PinManager::preCheck(int temp, unsigned long nowMS)
{
    // thread-safe: arbeitet AUF mit lokalen Kopien (aus pidLockRead())
    LOG_DEBUG("PinManager::PID: : %s", pcTaskGetName(NULL));
    // allowed boiler temp
    if (temp >= m_tempMaxAllowed)
    {
        LOG_DEBUG(TAG_PID, "PID: Max Temperatur <%d> erreicht: <%d>, abschalten", m_tempMaxAllowed, temp);
        reset();
        return MODE_OFF;
    }

    #ifdef LEGIONELLA
    // LEGIONELLA
    if (nowMS - lastLegionella > m_legionellenDelta /*7UL * 24 * 3600 * 1000*/)
        legionella = true;

    if (legionella)
    {
        if (temp >= m_legionellenMaxTemp /*LEG_TEMP*/)
        {
            legionella = false;
            lastLegionella = nowMS;
            char tempBuf[10];  // Platz für "-123.45\0"
            char tempBuf1[10]; // Platz für "-123.45\0"
            LOG_DEBUG(TAG_PID, "PID: Legionellen Temperatur <%s> erreicht: <%s>", fToStr(m_legionellenMaxTemp, 5, 1, tempBuf), fToStr(temp, 5, 1, tempBuf1));
            return MODE_OFF;
        }
        else
        {

            LOG_DEBUG(TAG_PID, "PID:HEAT (Legionellen)");
            return MODE_LEGIONELLA;
        }
    }
    #endif
    if (temp < m_tempMin)
    {
        LOG_DEBUG(TAG_PID, "PID: Min Temperatur <%d> erreicht: <%d>, einschalten", m_tempMin, temp);
        powerIndex = 0;
        return MODE_MIN_TEMP;
    }

    if (m_boilerHeating != HEATING_AUTOMATIC)
    // LEGIONELLetupData.forceHeating != HEATING_AUTOMATIC) // no pid controller, all is forced
    {
        LOG_DEBUG(TAG_PID, "PID  Manuelle Steuerung - keine Automatik");
        powerIndex = 0;
        return MODE_MANUAL; // nothing must be done due to overruling everything
    }

    int availableWatt;
    //  <0:  einspeisen, >0: Bezug
    // m_meterPower ist harmonisiert (Fronius p_load oder AMIS consumptionInWatt)
    // Fronius mit Akku-Prioritäten: extra Logik für Ladezustand
    if (m_froniusAPI)
    {
        if (m_akkuLadung < 20.0f)
        { // <0: laden, >0 entladen
            if (m_akkuPriori == AKKU_PRIORITY_SUBORDINATED)
            {
                availableWatt = (int)(m_akkuLadung + m_gridPower);
                LOG_DEBUG(TAG_PID, "PID (Fronius) Akku nachrangig, available Watt: %d", availableWatt);
            }
            else
            {
                availableWatt = (int)m_gridPower;
                LOG_DEBUG(TAG_PID, "PID (Fronius) Akku vorrangig, available Watt: %d", availableWatt);
            }
        }
        else
        {
            availableWatt = (int)m_gridPower;
            LOG_INFO(TAG_PID, "PID (Fronius) Available Watt: %d", availableWatt);
        }
    }
    else
    {
        // Harmonisierter Wert – kommt vom AMIS-Reader (<0: Einspeisung, >0: Bezug)
        availableWatt = (int)m_meterPower;
        LOG_INFO(TAG_PID, "PID Harmonisiert (AMIS/Modbus) AvailableWatt: %d", availableWatt);
    }

    // WATT-Bias for testing - only for testing
    if (m_wattSetupForTest != 0)
    {
        m_out_wattBiasForTest = true;
        availableWatt = m_wattSetupForTest;
        LOG_DEBUG(TAG_PID, "PID TEST MODE - AvailableWatt overridden by setup: %d", availableWatt);
    }
    else
    {
        m_out_wattBiasForTest = false;
    }

    // NO PV → minimal heating via RL
    if (powerIndex < HYSTERESIS_WATT)
    {
        availablePower.push_back(availableWatt); // availablePower;
        ++powerIndex;
        LOG_DEBUG(TAG_PID, "PinManager::preCheck - powerIndex < MAX_LEN_MEASUR: %d, powerIndex: %d", availableWatt, powerIndex);
    }
    else
    {
        // LOG_INFO("PinManager::preCheck - Means of HYSTERESIS_WATT-2 measures  %f", availableWatt);
        powerIndex = 0;
        availablePower.push_back(availableWatt); // availablePower;
        ++powerIndex;
    }
    /* LOG_DEBUG(TAG_PID, "PinManager::preCheck - EXIT: %f, powerIndex: %d", availableWatt, powerIndex); */
    return MODE_AUTO;
}

#define ABS(N) ((N < 0) ? (-N) : (N))

void PinManager::update(WEBSOCK_DATA &webSockData /*, double temp, int hour*/)
{
    unsigned long now = millis();
    LogEntry logEntry{};
    time_t curT;
    time(&curT);

    // ── STEP 1: thread-safe read all inputs into local members ──
    pidLockRead(webSockData);

    m_out_boilerHeating = false; // default: heater off

    // ── TEMPERATURVALIDIERUNG (thread-safe: local copies) ──
    int temp = (m_sensor1 + m_sensor2) / 2;
    if (m_sensor1 < 0) temp = m_sensor2;
    if (m_sensor2 < 0) temp = m_sensor1;
    if (temp <= 0)
    {
        LOG_ERROR(TAG_PID, "Ungültige Temperaturmessung: sensor1: %d, sensor2: %d", m_sensor1, m_sensor2);
        reset();
        pidLockWrite(webSockData);
        utils_logWrite(webSockData.logBuffer, logEntry); // outside appLock → no R01 deadlock
        return;
    }
    // ── ENDE TEMPERATURVALIDIERUNG ──

    logEntry.temp = temp;
    logEntry.ts = curT;

    m_froniusAPI = webSockData.states.froniusAPI; // Fronius API verfügbar?
    // ── STEP 2: core logic (thread-safe: only local members) ──
    ControlMode currentMode = preCheck(temp, now);
    int measuredPower = 0;
    int targetPower = 0;
    int action = 0;

    bool doML = true;
    LOG_INFO(TAG_PID, "PinManager::update() - currentMode: %d, temperature %d, sensor1 %d, sensor2 %d",
              currentMode, temp, m_sensor1, m_sensor2);

    switch (currentMode)
    {
    case MODE_OFF:
        targetPower = 0;
        measuredPower = 0;
        logEntry.power = 0;
        logEntry.pwm = 0;
        currentPWM = 0;

        logEntry.state = 0;
        targetPower = 0;

        doML = false;
        reset(); // Interne Zähler zurücksetzen
        LOG_INFO(TAG_PID, "HEAT OFF (Sicherheit) - Alle Relais aus, PWM 0");
        break;

    case MODE_LEGIONELLA:
    case MODE_MIN_TEMP:
        targetPower = measuredPower = onePhase * 3; // Volle Kraft
        doML = false;
        currentPWM = 255;

        logEntry.state = 3; // Spezieller Zustand für Legionella
        logEntry.power = targetPower;
        logEntry.pwm = currentPWM;

        LOG_INFO(TAG_PID, "HEAT ON (Sicherheit) - Alle Relais ein, PWM 254");
        break;

    case MODE_MANUAL:

        // Hier einfach den aktuellen Ist-Wert lassen oder aus webSockData lesen
        LOG_INFO(TAG_PID, "Manuelle Steuerung");
        /*
        logEntry.state = (digitalRead(pinL1) == HIGH) || (digitalRead(pinL2) == HIGH);
        logEntry.pwm = (int)currentPWM;
        logEntry.power = PinManager::preCheck - powerIndex < MAX_LEN_MEASURgetMeanOfAvailAblePower();
        */
        doML = false;
        // fillLogEntry(webSockData, logEntry);
        targetPower = onePhase * 3;
        availablePower.clear();
        break;

    case MODE_AUTO:
    {
        // Nur hier läuft deine RL-Logik!doML = false;
        doML = true;

        measuredPower = getMeanOfAvailAblePower();
        LOG_DEBUG(TAG_PID, "RL AUTO - Gemessene Leistung: %d W ", measuredPower);
        logEntry.power = measuredPower;
        if (abs(measuredPower) < EPSILON_TEMP || measuredPower > 0)
        {
            targetPower = 0; // <--- DAS schaltet aus, wenn kein Strom da ist!

            LOG_INFO(TAG_PID, "PID eXIT true, AvailableWatt: %d < %d (Epsilon)", (int)measuredPower, (int)EPSILON_TEMP);
            targetPower = 0; // <--- DAS schaltet aus, wenn kein Strom da ist!
            doML = false;
        }
        else
        {

            // 1. Aktuelle Gesamtsituation erfassen
            int heater = heaterPower();
            doML = true;
            // Was wir theoretisch verbrauchen könnten (Überschuss + aktueller Eigenverbrauch)
            int effectiveAvailable = (-measuredPower) + heater;
            LOG_DEBUG(TAG_PID, "RL AUTO - Effektive Leistung: %d W ", effectiveAvailable);


            // BOILER: 2 Phasen werden per Relais geschaltet, die dritte Phase per PWM.
            // Der komplette verfügbare Überschuss wird als Zielleistung genutzt;
            // die Relais-Hysterese und der PWM-Rest werden in apply() umgesetzt.
            targetPower = clampInt(effectiveAvailable, 0, onePhase * 3);
            action = (int)(targetPower / onePhase);


            if (targetPower > effectiveAvailable + 50)
            {
                targetPower = effectiveAvailable;
            }
            /*  LOG_INFO(TAG_PID, "ML Calc: Avail: %dW, Ph: %d, PWM-Base: %dW, Factor: %.2f -> Target: %dW",
                      effectiveAvailable, fullPhases, remainingForPWM, chosenFactor, targetPower); */

        } // else

        break;
    } // case
    } // switch

    LOG_INFO(TAG_PID, "before calling apply, AvailableWatt: %d ", (int)targetPower);
    if (targetPower > 0)
    {
        m_out_boilerHeating = true;
    }
    apply(logEntry, targetPower);

    vTaskDelay(pdMS_TO_TICKS(50)); // "Atempause"

    // Prüfe, wie viel Stack noch übrig ist (in Bytes)
    UBaseType_t stackLeft = uxTaskGetStackHighWaterMark(NULL);
    // LOG_INFO(TAG_PID, "Freier Stack vor Write: %u, Task: %s", stackLeft * sizeof(StackType_t), pcTaskGetTaskName(NULL));

    if (stackLeft < 200)
    { // Willkürliche Grenze
        LOG_ERROR(TAG_PID, "STACK FAST VOLL! Aufruf wird wahrscheinlich crashen.");
    };

    // ── STEP 3: thread-safe write outputs, then log outside lock ──
    pidLockWrite(webSockData);               // writes pidContainer + boilerHeating under appLock
    utils_logWrite(webSockData.logBuffer, logEntry); // outside appLock → prevents R01 ABBA deadlock

    /*
    Skalierte Strafe: Anstatt nur -2 zu geben, wenn Strom bezogen wird, bestrafst du hohen Bezug stärker. Das lehrt den Algorithmus, bei knapper PV-Leistung eher vorsichtig zu sein.

    Sweet Spot Bonus: Der Agent bekommt eine hohe Belohnung, wenn measuredPower nahe bei 0 liegt. Das ist das Ziel: Den Hausanschluss auf 0W zu halten.

    Vermeidung von Extremen: Wenn die Temperatur zu hoch wird, sinkt der Reward, sodass der Agent lernt, die Leistung rechtzeitig zu drosseln, bevor der preCheck (Sicherheit) hart abschaltet.
    */
    LOG_DEBUG(TAG_PID, "ENTER ML Task: %s", pcTaskGetTaskName(NULL));

    /*    int ts = tempState(temp);
       int ps = pvState(measuredPower);
       int action = chooseAction(ts, ps);
    */
    if (doML)
    {

        float reward = 0.0; // Nutze float für feinere Abstufung

        // 1. Der "Sweet Spot" (Netz-Null-Punkt)
        // Wir bestrafen sowohl Einspeisung als auch Bezug,
        // aber Bezug (Strom kaufen) ist teurer/schlechter.
        if (measuredPower > 0)
        {
            // Netzbezug: Strafe skaliert mit der Leistung
            reward -= (measuredPower / 50.0);
        }
        else if (measuredPower < 0)
        {
            // Überschuss vorhanden:
            // Wenn wir nah an der Null sind (z.B. -10 bis -50W), gibt es einen Bonus.
            if (measuredPower > -50)
                reward += 10.0;
            else
                reward += 2.0; // Kleiner Bonus für generelle Nutzung von Überschuss
        }

        // 2. Temperatur-Management
        // Ziel: 57°C (dein Setpoint im Code)
        float tempDiff = abs(temp - 57);
        if (tempDiff < 2)
        {
            reward += 5.0; // Voller Bonus bei Zieltemperatur
        }
        else
        {
            reward -= (tempDiff * 0.5); // Abzug, je weiter wir weg sind
        }

        // 3. Hardware-Schonung (Relais-Check)
        // Wenn die Action einen Phasenwechsel erzwingt, geben wir einen kleinen Abzug,
        // damit das Netz lernt, nur zu schalten, wenn es sich wirklich lohnt.
        static int lastActionPhases = 0;
        int currentActionPhases = (int)(targetPower / onePhase);
        if (currentActionPhases != lastActionPhases)
        {
            reward -= 5.0; // "Schaltkosten"
        }
        lastActionPhases = currentActionPhases;

        // 4. Sicherheit (Extremwerte)
        if (temp > (m_tempMaxAllowed - 2))
        {
            reward -= 20.0; // Massive Strafe kurz vor Not-Aus
        }

        // Übergabe an das neuronale Netz
        tinyNN->remember(temp, measuredPower, action, (int)reward);
        tinyNN->trainReplay();
        LOG_INFO(TAG_PID, "EXIT ML Task: %s", pcTaskGetTaskName(NULL));
    } // doML
}

/*
****** aPPLY
*/

void PinManager::apply(LogEntry &logEntry, int targetPower)
{
    LOG_INFO(TAG_PID, "PinManager::apply() - ENTER Task %s, available watt: %d", pcTaskGetName(NULL), targetPower);
    unsigned long now = millis();
    targetPower = clampInt(targetPower, 0, onePhase * 3);

    // 🔥 HARD STOP
    if (targetPower < 50)
    {
        digitalWrite(pinL1, LOW);
        digitalWrite(pinL2, LOW);
        analogWrite(pwmPin, 0);
        currentPWM = 0;

        logEntry.state = 0;
        logEntry.pwm = 0;
        logEntry.power = 0;
        return;
    }

    // 2 Relaisphasen plus eine PWM-Phase. Relais schalten nur bei voller
    // Phasenleistung plus Hysterese, damit sie am Schwellwert nicht flattern.
    const int margin = onePhase / 10;
    int currentActive = (digitalRead(pinL1) == HIGH ? 1 : 0) + (digitalRead(pinL2) == HIGH ? 1 : 0);
    int desiredPhases = currentActive;

    if (desiredPhases < 1 && targetPower >= (onePhase + margin))
    {
        desiredPhases = 1;
    }
    else if (desiredPhases >= 1 && targetPower <= (onePhase - margin))
    {
        desiredPhases = 0;
    }

    if (desiredPhases < 2 && targetPower >= ((2 * onePhase) + margin))
    {
        desiredPhases = 2;
    }
    else if (desiredPhases >= 2 && targetPower <= ((2 * onePhase) - margin))
    {
        desiredPhases = 1;
    }

    LOG_INFO(TAG_PID, "apply (1) - targetPower: %d, desiredPhases: %d, currentActive: %d", targetPower, desiredPhases, currentActive);

    // 3. Relais schalten (mit Zeitverzögerung MIN_SWITCH gegen Verschleiß)
    if (desiredPhases != currentActive && (now - lastSwitch > MIN_SWITCH))
    {
        /*  LOG_INFO(TAG_PID, "apply (1) write to port- targetPower: %d, desiredPhases: %d, currentActive: %d", targetPower, desiredPhases, currentActive); */
        digitalWrite(pinL1, desiredPhases >= 1 ? HIGH : LOW);
        digitalWrite(pinL2, desiredPhases >= 2 ? HIGH : LOW);
        lastSwitch = now;
        currentActive = desiredPhases;
    }

    int powerFromRelays = currentActive * onePhase;
    int powerForPWM = clampInt(targetPower - powerFromRelays, 0, onePhase);

    currentPWM = wattToPwm(powerForPWM, onePhase);
    analogWrite(pwmPin, currentPWM);

    // Logging
    logEntry.pwm = currentPWM;
    logEntry.power = powerFromRelays + powerForPWM;
    int state = 0;
    if (digitalRead(pinL1) == HIGH)
        state |= 1; // Setzt Bit 0
    if (digitalRead(pinL2) == HIGH)
        state |= 2; // Setzt Bit 1
    logEntry.state = state;

    LOG_INFO(TAG_PID, "Relais 1→ %d, Relais 2→ %d, pwm→ %d, pwmWatt→ %d", currentActive >= 1, currentActive >= 2, currentPWM, powerForPWM);
    LOG_INFO(TAG_PID, "Status Bitmaske: %d (L1: %d, L2: %d)",
             state, (state & 1), (state >> 1 & 1));



    LOG_INFO(TAG_PID, "PinManager::apply() - EXIT Task %s", pcTaskGetName(NULL));
}

/*
******* HELPER
*/
int PinManager::heaterPower()
{
    int p = 0;

    if (digitalRead(pinL1))
        p += onePhase;
    if (digitalRead(pinL2))
        p += onePhase;

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
    if (pin == 0)
        return (digitalRead(pinL1) ? 1 : 0);
    if (pin == 1)
        return (digitalRead(pinL2) ? 1 : 0);

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
    currentPWM = OUTPUT_MAX;
}
inline int PinManager::getMeanOfAvailAblePower()
{
    // 1. Fensterverwaltung: Ältesten Wert entfernen, wenn Puffer voll
    // LOG_DEBUG(TAG_PID, "getMeanOfAvailAblePower - ENTER: %d", (int)availablePower.size());

    if (availablePower.size() > HYSTERESIS_WATT)
    {
        availablePower.erase(availablePower.begin());
    }

    size_t n = availablePower.size();

    // 2. Fallback: Zu wenig Daten für Ausreißer-Bereinigung
    if (n < 3)
    {
        if (n == 0)
            return 0.0;
        int sum = 0;
        for (int v : availablePower)
            sum += v;
        // LOG_DEBUG(TAG_PID, "SuM %d values", (int) sum, n);
        return sum / n;
    }

    // 3. Kopie erstellen und sortieren (wir wollen das Originalfenster nicht zerstören)
    std::vector<int> sortedValues = availablePower;
    std::sort(sortedValues.begin(), sortedValues.end());

    // 4. Trimmed Mean: Ersten und letzten Wert ignorieren
    double sum = 0;
    for (size_t i = 1; i < n - 1; i++)
    {
        sum += sortedValues[i];
    }
    // LOG_DEBUG(TAG_PID, "SuM 2  %d values", (int)sum, n);
    //  Division durch (n - 2), da wir zwei Werte entfernt haben
    return sum / (n - 2);
}
