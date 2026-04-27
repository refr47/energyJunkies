#include "pinManager.h"
#include "app_state.h"
#include "utils.h"

void PinManager::config(WEBSOCK_DATA &data, int l1, int l2, int pwm)
{
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
inline ControlMode PinManager::preCheck(WEBSOCK_DATA &webSockData, int temp, unsigned long nowMS)
{
    // SAFETY,
    LOG_DEBUG("PinManager::PID: : %s", pcTaskGetName(NULL));
    // allowed boiler temp
    if (temp >= webSockData.setupData.tempMaxAllowedInGrad)
    {

        LOG_DEBUG(TAG_PID, "PID: Max Temperatur <%d> erreicht: <%d>, abschalten", webSockData.setupData.tempMaxAllowedInGrad, temp);
        reset();

        return MODE_OFF;
    }
#ifdef BOILER
    // LEGIONELLA
    if (nowMS - lastLegionella > webSockData.setupData.legionellenDelta /*7UL * 24 * 3600 * 1000*/)
        legionella = true;

    if (legionella)
    {
        if (temp >= webSockData.setupData.legionellenMaxTemp /*LEG_TEMP*/)
        {
            legionella = false;
            lastLegionella = nowMS;
            char tempBuf[10];  // Platz für "-123.45\0"
            char tempBuf1[10]; // Platz für "-123.45\0"
            LOG_DEBUG(TAG_PID, "PID: Legionellen Temperatur <%s> erreicht: <%s>", fToStr(webSockData.setupData.legionellenMaxTemp, 5, 1, tempBuf), fToStr(temp, 5, 1, tempBuf1));
            return MODE_OFF;
        }
        else
        {

            LOG_DEBUG(TAG_PID, "PID:HEAT (Legionellen)");
            return MODE_LEGIONELLA;
        }
    }
    if (temp < webSockData.setupData.tempMinInGrad)
    {

        LOG_DEBUG(TAG_PID, "PID: Min Temperatur <%d> erreicht: <%d>, einschalten", webSockData.setupData.tempMinInGrad, temp);
        powerIndex = 0;
        return MODE_MIN_TEMP;
    }
#endif
    if (webSockData.setupData.forceHeating != HEATING_AUTOMATIC) // no pid controller, all is forced
    {
        LOG_DEBUG(TAG_PID, "PID  Manuelle Steuerung - keine Automatik");
        powerIndex = 0;

        return MODE_MANUAL; // nothing must be done due to overruling everything
    }

    int availableWatt;
    //  <0:  einspeisen, >0: Bezug
    if (webSockData.states.froniusAPI)
    {
        if (webSockData.fronius_SOLAR_POWERFLOW.p_akku < 20.0)
        { // <0: laden, >0 entladen
            if (webSockData.setupData.akkuPriori == AKKU_PRIORITY_SUBORDINATED)
            {
                availableWatt = (int)(webSockData.fronius_SOLAR_POWERFLOW.p_akku + webSockData.fronius_SOLAR_POWERFLOW.p_grid); // gird < 0: einspeisen, > 0 bezug

                LOG_DEBUG(TAG_PID, "PID (Fronius) Akku priority subordinated (nachrangig), available Watt: %d", availableWatt);
            }
            else
            {

                LOG_DEBUG(TAG_PID, "PID (Fronius) Akku priority primary (vorrangig)available Watt: %d", availableWatt);
                availableWatt = (int)webSockData.fronius_SOLAR_POWERFLOW.p_grid;
            }
        }
        else
        {
            availableWatt = (int)webSockData.fronius_SOLAR_POWERFLOW.p_grid;

            LOG_DEBUG(TAG_PID, "PID (Fronius) Available Watt: %d", availableWatt);
        }
    }
    else
    {
        // gwebSockData.mbContainer.meterValues.data.acCurrentPower < 0: export: else import
        availableWatt = (int)webSockData.mbContainer.meterValues.data.acCurrentPower;

        LOG_DEBUG(TAG_PID, "PID No froniusAPI) AvailableWatt: %d", availableWatt);
    }

    // WATT-Bias for testing - only for testing
    if (webSockData.setupData.wattSetupForTest != 0)
    {
        webSockData.states.wattBiasForTest = true;
        availableWatt = webSockData.setupData.wattSetupForTest;

        LOG_DEBUG(TAG_PID, "PID TEST MODE - AvailableWatt overridden by setup: %d", availableWatt);
    }
    else
    {
        webSockData.states.wattBiasForTest = false;
    }
    // NO PV → minimal heating via RL
    /*   if (availableWatt > 0.0)
      {
          LOG_DEBUG("PinManager::preCheck - Bezug <%.3f>", availableWatt);
          return true;
      } */
    /* LOG_DEBUG(TAG_PID, "PinManager::after FRONIUS - powerindex: %d HYSTERESIS_WATT: %d", powerIndex, HYSTERESIS_WATT);
     */
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

void inline PinManager::fillLogEntry(WEBSOCK_DATA &webSockData, LogEntry &logEntry)
{
    utils_logWrite(webSockData.logBuffer, logEntry);
    // LOG_DEBUG(TAG_PID, "===> LogEntry ts: %lu, temp: %d, power: %d, pwm: %d, state: %d", logEntry.ts, logEntry.temp, logEntry.power, logEntry.pwm, logEntry.state);
    webSockData.pidContainer.mAnalogOut = currentPWM;
    webSockData.pidContainer.PID_PIN1 = digitalRead(pinL1) == HIGH ? 1 : 0;
    webSockData.pidContainer.PID_PIN2 = digitalRead(pinL2) == HIGH ? 1 : 0;
}

void PinManager::update(WEBSOCK_DATA &webSockData /*, double temp, int hour*/)
{
    unsigned long now = millis();
    LogEntry logEntry;
    time_t curT;
    time(&curT);
    webSockData.states.boilerHeating = false;

    int temp = (webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2;
    if (webSockData.temperature.sensor1 < 0)
    {
        temp = webSockData.temperature.sensor2;
    }
    if (webSockData.temperature.sensor2 < 0)
    {
        temp = webSockData.temperature.sensor1;
    }
    if (temp <= 0)
    {

        LOG_ERROR(TAG_PID, "Ungültige Temperaturmessung: sensor1: %d, sensor2: %d", webSockData.temperature.sensor1, webSockData.temperature.sensor2);
        return;
    }

    // logEntry.tag = "PID";
    logEntry.temp = temp;
    logEntry.ts = curT;
    // LOG_ERROR(TAG_PID, "PinManager::BEFORE() - Task: %s", pcTaskGetName(NULL));
    ControlMode currentMode = preCheck(webSockData, temp, now);
    // LOG_ERROR(TAG_PID, "PinManager::AFTER() - Task: %s", pcTaskGetName(NULL));
    int measuredPower = 0;
    int targetPower = 0;
    int action = 0;

    bool doML = true;
    LOG_ERROR(TAG_PID, "PinManager::update() - currentMode: %d, temperature %d, sensor1 %d, sensor2 %d", currentMode, temp, webSockData.temperature.sensor1, webSockData.temperature.sensor2);

    switch (currentMode)
    {
    case MODE_OFF:
        targetPower = 0;
        measuredPower = 0;
        logEntry.power = 0;
        logEntry.pwm = 0;

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
        logEntry.state = 3; // Spezieller Zustand für Legionella
        logEntry.power = targetPower;
        logEntry.pwm = 255;

        LOG_INFO(TAG_PID, "HEAT ON (Sicherheit) - Alle Relais ein, PWM 254");
        break;

    case MODE_MANUAL:

        // Hier einfach den aktuellen Ist-Wert lassen oder aus webSockData lesen
        LOG_INFO(TAG_PID, "Manuelle Steuerung");
        /*
        logEntry.state = (digitalRead(pinL1) == HIGH) || (digitalRead(pinL2) == HIGH);
        logEntry.pwm = (int)currentPWM;
        logEntry.power = getMeanOfAvailAblePower();
        */
        doML = false;
        // fillLogEntry(webSockData, logEntry);
        targetPower = 15000;
        availablePower.clear();
        break;

    case MODE_AUTO:
    {
        // Nur hier läuft deine RL-Logik!doML = false;
        doML = true;

        measuredPower = getMeanOfAvailAblePower();
        LOG_DEBUG(TAG_PID, "RL AUTO - Gemessene Leistung: %d W ", measuredPower);
        logEntry.power = measuredPower;
        if (abs(measuredPower) < EPSILON_TEMP)
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
#ifdef BOILER
           
            // 2. Deterministische Basis (Wie viele Phasen sind VOLLSTÄNDIG deckbar?)
            int fullPhases = (int)(effectiveAvailable / onePhase);
            if (fullPhases > 2)
                fullPhases = 2; // L1 und L2 sind Relais
            if (fullPhases < 0)
                fullPhases = 0;

            // Was bleibt für die PWM-Phase (L3) übrig?
            int remainingForPWM = effectiveAvailable - (fullPhases * onePhase);
            if (remainingForPWM < 0)
                remainingForPWM = 0;

            // 3. 🧠 TinyNN entscheidet NUR über den variablen Anteil (die PWM-Phase)
            action = tinyNN->chooseAction(temp, measuredPower);

            // WICHTIG: factor als float, damit die Multiplikation unten nicht 0 ergibt!
            float factors[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
            float chosenFactor = factors[action];

            // 4. Zielwert berechnen
            // Die Relais-Phasen nehmen wir fix (basierend auf Überschuss),
            // die PWM-Phase wird vom ML-Agenten feinjustiert.
            targetPower = (fullPhases * onePhase) + (int)(onePhase * chosenFactor);
#endif
#ifdef PHASEN2
          
            int fullPhases = effectiveAvailable / onePhase;
            if (fullPhases > 1)
                fullPhases = 1; // Nur ein Relais vorhanden!

            action = tinyNN->chooseAction(temp, measuredPower);
            float factors[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

            // TargetPower = (1000W wenn genug da) + (0-1000W per PWM)
            targetPower = (fullPhases * onePhase) + (int)(onePhase * factors[action]);

#endif
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
        webSockData.states.boilerHeating = true;
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
    fillLogEntry(webSockData, logEntry);

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
        if (temp > (webSockData.setupData.tempMaxAllowedInGrad - 2))
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


    // 🔥 HARD STOP
    if (targetPower < 50)
    {
        digitalWrite(pinL1, LOW);
        digitalWrite(pinL2, LOW);
        analogWrite(pwmPin, 0);

        logEntry.state = 0;
        logEntry.pwm = 0;
        logEntry.power = 0;
        return;
    }
#ifdef BOILER
    // 2. Phasen-Logik mit Hysterese
    // Wir bestimmen, wie viele Phasen VOLL (per Relais) laufen sollen.
    const int margin = onePhase * 0.05;
    int currentActive = (digitalRead(pinL1) == HIGH ? 1 : 0) + (digitalRead(pinL2) == HIGH ? 1 : 0);
    int desiredPhases = currentActive;

    // Einschalt-Schwellen (mit 10% Puffer nach oben)
    // --- Logik für Phase 1 (L1) ---
    if (currentActive == 0 && targetPower > (onePhase + margin))
    {
        desiredPhases = 1; // Einschalten wenn über Schwelle + Puffer
    }
    else if (currentActive == 1 && targetPower < (onePhase - margin))
    {
        desiredPhases = 0; // Ausschalten wenn unter Schwelle - Puffer
    }

    // --- Logik für Phase 2 (L2) ---
    if (currentActive == 1 && targetPower > (2 * onePhase + margin))
    {
        desiredPhases = 2;
    }
    else if (currentActive == 2 && targetPower < (2 * onePhase - margin))
    {
        desiredPhases = 1;
    }

    LOG_INFO(TAG_PID, "apply (1) - targetPower: %d, desiredPhases: %d, currentActive: %d", targetPower, desiredPhases, currentActive);

    // 3. Relais schalten (mit Zeitverzögerung MIN_SWITCH gegen Verschleiß)
    if (desiredPhases != currentActive && (now - lastSwitch > MIN_SWITCH))
    {
       /*  LOG_INFO(TAG_PID, "apply (1) write to port- targetPower: %d, desiredPhases: %d, currentActive: %d", targetPower, desiredPhases, currentActive); */
        digitalWrite(pinL1, desiredPhases >= 1);
        digitalWrite(pinL2, desiredPhases >= 2);
        lastSwitch = now;
        currentActive = desiredPhases;
    }

    // 4. PWM Berechnung für die REST-Leistung (L3)
    // Wir ziehen die Leistung der eingeschalteten Relais von der Ziel-Leistung ab
    int powerFromRelays = currentActive * onePhase;
    int powerForPWM = targetPower - powerFromRelays;

    // Begrenzung der PWM-Leistung auf eine Phasenstärke
    if (powerForPWM < 0)
        powerForPWM = 0;
    if (powerForPWM > onePhase)
        powerForPWM = onePhase;

    // WICHTIG: Fließkomma-Berechnung oder erst multiplizieren, dann dividieren!
    float pwmDutyCycle = ((float)powerForPWM / (float)onePhase) * OUTPUT_MAX;

    // Glättung (optional)
    currentPWM = (0.6 * currentPWM) + (0.4 * pwmDutyCycle);

    analogWrite(pwmPin, (int)currentPWM);

    // Logging
    logEntry.pwm = (int)currentPWM;
    int state = 0;
    if (digitalRead(pinL1) == HIGH)
        state |= 1; // Setzt Bit 0
    if (digitalRead(pinL2) == HIGH)
        state |= 2; // Setzt Bit 1
    logEntry.state = state;

    LOG_INFO(TAG_PID, "Relais 1→ %d, Relais 2→ %d, pwm→ %d", desiredPhases >= 1, desiredPhases >= 2, (int)currentPWM);
    LOG_INFO(TAG_PID, "Status Bitmaske: %d (L1: %d, L2: %d)",
             state, (state & 1), (state >> 1 & 1));
#endif
#ifdef PHASEN2
    const float P_ph = (float)onePhase;
    const float margin = P_ph * 0.10;       // 10% Hysterese (z.B. 100W bei 1000W Phase)
    const unsigned long relayDelay = 10000; // 10 Sek. Beruhigungszeit

    // 1. Relais-Logik (Hysterese)
    // Wir nutzen 'targetPower' direkt als Entscheidungsgrundlage
    if (!relayState)
    {
        // EINSCHALTEN: Wenn die Wunschleistung deutlich über einer Phase liegt
        if (targetPower > (P_ph + margin))
        {
            if (relayCandidateTimer == 0)
                relayCandidateTimer = now;
            if (now - relayCandidateTimer > relayDelay)
            {
                relayState = true;
                relayCandidateTimer = 0;
                lastSwitch = now; // Zeitstempel für Hardware-Schutz
            }
        }
        else
        {
            relayCandidateTimer = 0;
        }
    }
    else
    {
        // AUSSCHALTEN: Wenn die Wunschleistung unter die Phase minus Puffer fällt
        if (targetPower < (P_ph - margin))
        {
            if (relayCandidateTimer == 0)
                relayCandidateTimer = now;
            if (now - relayCandidateTimer > relayDelay)
            {
                relayState = false;
                relayCandidateTimer = 0;
                lastSwitch = now;
            }
        }
        else
        {
            relayCandidateTimer = 0;
        }
    }

    // 2. PWM-Berechnung (Restwert-Regelung)
    float P_for_pwm = 0;
    if (relayState)
    {
        // Relais liefert die erste Phase (fix), PWM liefert den Rest
        P_for_pwm = (float)targetPower - P_ph;
    }
    else
    {
        // Relais ist aus, PWM übernimmt die komplette targetPower (bis max P_ph)
        P_for_pwm = (float)targetPower;
    }

    // Begrenzung auf 0 bis onePhase
    P_for_pwm = constrain(P_for_pwm, 0, P_ph);

    // Duty Cycle berechnen (0-255)
    int finalPWM = (int)((P_for_pwm / P_ph) * 255);

    // Hardware-Output
    digitalWrite(pinL1, relayState); // Phase 1
    analogWrite(pwmPin, finalPWM);   // Phase 3 (PWM)

    // 3. LogEntry für das Frontend befüllen
    logEntry.pwm = finalPWM;
    logEntry.power = targetPower;

    // Bitmaske: Bit 0 für Relais L1
    int state = 0;
    if (relayState)
        state |= 1;
    // Falls L2 dauerhaft an ist (Stern ohne N), könntest du hier Bit 1 setzen:
    // state |= 2;
    logEntry.state = state;

    LOG_INFO(TAG_PID, "Apply Stern: Target %dW, Relay %s, PWM %d",
             targetPower, relayState ? "AN" : "AUS", finalPWM);

#endif

    LOG_INFO(TAG_PID, "PinManager::apply() - EXIT Task %s", pcTaskGetName(NULL));
}
#ifdef PHASEN2
void PinManager::setRelaySafe(int pin, bool state, unsigned long &lastSwitch)
{
    unsigned long now = millis();

    if (digitalRead(pin) != state && (now - lastSwitch > MIN_RELAY_SWITCH))
    {
        digitalWrite(pin, state);
        lastSwitch = now;
    }
}
#endif
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

    p += (currentPWM / OUTPUT_MAX) * onePhase;

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