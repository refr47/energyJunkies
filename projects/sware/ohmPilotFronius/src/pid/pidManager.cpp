#include "pinManager.h"
#include "app_state.h"
#include "utils.h"

void PinManager::config(WEBSOCK_DATA &data, int l1, int l2, int pwm)
{
    onePhase = data.setupData.heizstab_leistung_in_watt / 3.0;

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

    /* for (int i = 0; i < S_T; i++)
        for (int j = 0; j < S_P; j++)
            for (int k = 0; k < A; k++)
                Q[i][j][k] = 0; */
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
double PinManager::basePower(double effective)
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

ControlMode PinManager::preCheck(WEBSOCK_DATA &webSockData, double temp, unsigned long nowMS, LogEntry &logEntry)
{
    // SAFETY,
    LOG_DEBUG("PinManager::PID: : %s", pcTaskGetName(NULL));
    // allowed boiler temp
    if (temp >= webSockData.setupData.tempMaxAllowedInGrad)
    {
        char tempBuf[10];  // Platz für "-123.45\0"
        char tempBuf1[10]; // Platz für "-123.45\0"

        LOG_DEBUG(TAG_PID, "PID: Max Temperatur <%s> erreicht: <%s>, abschalten", fToStr(webSockData.setupData.tempMaxAllowedInGrad, 10, 1, tempBuf), fToStr(temp, 5, 1, tempBuf1));
        reset();
        logEntry.state = 0; // L1 +L2 is on
        logEntry.pwm = 0;
        logEntry.power = 0;
        powerIndex = 0;
        return MODE_OFF;
    }

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
        char tempBuf[10];  // Platz für "-123.45\0"
        char tempBuf1[10]; // Platz für "-123.45\0"
        LOG_DEBUG(TAG_PID, "PID: Min Temperatur <%s> erreicht: <%s>, einschalten", fToStr(webSockData.setupData.tempMinInGrad, 5, 1, tempBuf), fToStr(temp, 5, 1, tempBuf1));
        powerIndex = 0;
        return MODE_MIN_TEMP;
    }

    if (webSockData.states.heating != HEATING_AUTOMATIC) // no pid controller, all is forced
    {
        LOG_DEBUG(TAG_PID, "PID  Manuelle Steuerung - keine Automatik");
        powerIndex = 0;

        logEntry.state = logEntry.state = (digitalRead(pinL1) == HIGH) || (digitalRead(pinL2) == HIGH);
        logEntry.pwm = (uint8_t)currentPWM;
        logEntry.power = (int16_t)rest;
        return MODE_MANUAL; // nothing must be done due to overruling everything
    }

    double availableWatt;
    //  <0:  einspeisen, >0: Bezug
    if (webSockData.states.froniusAPI)
    {
        if (webSockData.fronius_SOLAR_POWERFLOW.p_akku < 20.0)
        { // <0: laden, >0 entladen
            if (webSockData.setupData.externerSpeicherPriori == AKKU_PRIORITY_SUBORDINATED)
            {
                availableWatt = webSockData.fronius_SOLAR_POWERFLOW.p_akku + webSockData.fronius_SOLAR_POWERFLOW.p_grid; // gird < 0: einspeisen, > 0 bezug
                char tempBuf[20];                                                                                        // Platz für "-123.45\0"
                LOG_DEBUG(TAG_PID, "PID (Fronius) Akku priority subordinated (nachrangig), available Watt: %s", fToStr(availableWatt, 5, 1, tempBuf));
            }
            else
            {
                char tempBuf[20]; // Platz für "-123.45\0"
                LOG_DEBUG(TAG_PID, "PID (Fronius) Akku priority primary (vorrangig)available Watt: %s", fToStr(availableWatt, 5, 1, tempBuf));
                availableWatt = webSockData.fronius_SOLAR_POWERFLOW.p_grid;
            }
        }
        else
        {
            availableWatt = webSockData.fronius_SOLAR_POWERFLOW.p_grid;
            char tempBuf[20]; // Platz für "-123.45\0"
            LOG_DEBUG(TAG_PID, "PID (Fronius) Available Watt: %s", fToStr(availableWatt, 5, 1, tempBuf));
        }
    }
    else
    {
        // gwebSockData.mbContainer.meterValues.data.acCurrentPower < 0: export: else import
        availableWatt = webSockData.mbContainer.meterValues.data.acCurrentPower;
        char tempBuf[20]; // Platz für "-123.45\0"
        LOG_DEBUG(TAG_PID, "PID No froniusAPI) AvailableWatt: %s", fToStr(availableWatt, 5, 1, tempBuf));
    }
    // NO PV → minimal heating via RL
    /*   if (availableWatt > 0.0)
      {
          LOG_DEBUG("PinManager::preCheck - Bezug <%.3f>", availableWatt);
          return true;
      } */

    if (powerIndex < MAX_LEN_MEASURE)
    {
        availablePower.push_back(availableWatt); // availablePower;
        ++powerIndex;
    }
    else
    {
        // LOG_INFO("PinManager::preCheck - Means of MAX_LEN_MEASURE-2 measures  %f", availableWatt);
        powerIndex = 0;
    }

    return MODE_AUTO;
}
  
#define ABS(N) ((N < 0) ? (-N) : (N))

void inline PinManager:: fillLogEntry(WEBSOCK_DATA &webSockData, LogEntry &logEntry) {
    utils_logWrite(webSockData.logBuffer, logEntry);
    webSockData.pidContainer.mAnalogOut = currentPWM;
    webSockData.pidContainer.PID_PIN1 = digitalRead(pinL1) == HIGH ? 1 : 0;
    webSockData.pidContainer.PID_PIN2 = digitalRead(pinL2) == HIGH ? 1 : 0  ;
    
}

void PinManager::update(WEBSOCK_DATA &webSockData /*, double temp, int hour*/)
{
    unsigned long now = millis();
    LogEntry logEntry;
    time_t curT;
    time(&curT);

    int temp = (webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2;
    if (temp < 0)
    {
        // LOG_ERROR(TAG_PID, "PinManager::update() - Invalid temperature reading, skipping update, t1: %d t2: %d", webSockData.temperature.sensor1, webSockData.temperature.sensor2);
        // logEntry.tag = "PID";
        logEntry.temp = 0;
        logEntry.ts = (uint32_t)curT;
        logEntry.power = 0;
        logEntry.pwm = 0;
        logEntry.state = 0;
        fillLogEntry(webSockData, logEntry);
        return;
    }
    if (webSockData.temperature.sensor1 < 0)
    {
        temp = webSockData.temperature.sensor2;
    }
    if (webSockData.temperature.sensor2 < 0)
    {
        temp = webSockData.temperature.sensor1;
    }
 
    // logEntry.tag = "PID";
    logEntry.temp = temp;
    logEntry.ts = curT;
        // LOG_ERROR(TAG_PID, "PinManager::BEFORE() - Task: %s", pcTaskGetName(NULL));
        ControlMode currentMode = preCheck(webSockData, temp, now, logEntry);
    // LOG_ERROR(TAG_PID, "PinManager::AFTER() - Task: %s", pcTaskGetName(NULL));
    double measuredPower = 0.0;
    double targetPower = 0.0;
    bool doML = true;
    // LOG_ERROR(TAG_PID, "PinManager::update() - currentMode: %d, temperature %d, sensor1 %d, sensor2 %d", currentMode, temp, webSockData.temperature.sensor1, webSockData.temperature.sensor2);

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
        logEntry.power = targetPower;
        logEntry.pwm = 255;

        LOG_INFO(TAG_PID, "HEAT ON (Sicherheit) - Alle Relais ein, PWM 254");
        break;

    case MODE_MANUAL:
        // Hier einfach den aktuellen Ist-Wert lassen oder aus webSockData lesen
        LOG_INFO(TAG_PID, "Manuelle Steuerung");
        logEntry.state = (digitalRead(pinL1) == HIGH) || (digitalRead(pinL2) == HIGH);
        logEntry.pwm = (int)currentPWM;
        logEntry.power = getMeanOfAvailAblePower();
        doML = false;
        fillLogEntry(webSockData, logEntry);
        return;

    case MODE_AUTO:
        // Nur hier läuft deine RL-Logik!
        measuredPower = getMeanOfAvailAblePower();
        LOG_DEBUG(TAG_PID, "RL AUTO - Gemessene Leistung: %.3f W ", measuredPower);
        logEntry.power = measuredPower;
        if (abs(measuredPower) < EPSILON_TEMP)
        {
            targetPower = 0; // <--- DAS schaltet aus, wenn kein Strom da ist!

            char tempBuf[20];  // Platz für "-123.45\0"
            char tempBuf1[30]; // Platz für "-123.45\0"
            LOG_INFO(TAG_PID, "PID eXIT true, AvailableWatt: %s < %s (Epsilon)", fToStr(measuredPower, 5, 1, tempBuf), fToStr(EPSILON_TEMP, 5, 1, tempBuf1));
            targetPower = 0; // <--- DAS schaltet aus, wenn kein Strom da ist!
            doML = false;
        }
        else
        {
            double heater = heaterPower();
            double effective = (-measuredPower) + heater;

            // deterministic baseline
            // 🔢 physikalisch korrekt aufteilen
            int phases = (int)(effective / onePhase);
            if (phases > 2)
                phases = 2;
            if (phases < 0)
                phases = 0;

            double remaining = effective - phases * onePhase;

            // 🧠 TinyNN decides PWM factor
            int action = tinyNN->chooseAction(temp, measuredPower);

            double factors[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
            double factor = factors[action];

            // double pwmPower = remaining * factor;
            targetPower = phases * onePhase + (remaining * factor);

            // 🎯 reward (keep your improved version!)
            double reward = 0;

            if (measuredPower > 10.0)
                reward -= measuredPower / 200.0;
            else if (measuredPower < -50.0)
                reward += 0.5;

            double tempError = abs(temp - 57.5);
            reward += max(0.0, 2.0 - tempError * 0.2);

            reward += max(0.0, 3.0 - abs(measuredPower) / 20.0);

            // 🧠 learning
            tinyNN->remember(temp, measuredPower, action, reward);
            tinyNN->trainReplay();
        }

        break;
    }

    apply(logEntry, targetPower);

    vTaskDelay(pdMS_TO_TICKS(50)); // "Atempause"

    // Prüfe, wie viel Stack noch übrig ist (in Bytes)
    UBaseType_t stackLeft = uxTaskGetStackHighWaterMark(NULL);
    //LOG_INFO(TAG_PID, "Freier Stack vor Write: %u, Task: %s", stackLeft * sizeof(StackType_t), pcTaskGetTaskName(NULL));

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

        // reward
        double reward = 0;
        // 1. Netzbezug vermeiden (Hohe Strafe)
        // Jedes Watt Bezug (measuredPower > 0) wird bestraft.
        if (measuredPower > 10.0)
        {
            reward -= (measuredPower / 100.0); // Lineare Strafe: 500W Bezug = -5 Punkte
        }
        else if (measuredPower < -50.0)
        {
            reward += 1.0; // Bonus für echten Überschuss-Verbrauch
        }

        // 2. Zieltemperatur halten
        // Wir wollen zwischen 50°C und 65°C bleiben.
        if (temp >= 50.0 && temp <= 65.0)
        {
            reward += 2.0; // Wohlfühlbereich für den Boiler
        }
        else if (temp > webSockData.setupData.tempMaxAllowedInGrad - 5.0)
        {
            reward -= 3.0; // Strafe, wenn wir kurz vor dem Abschalten durch Überhitzung stehen
        }

        // 3. Effizienz-Bonus (RL soll "knapp" an der Nulllinie regeln)
        double netzDifferenz = abs(measuredPower);
        if (netzDifferenz < 50.0)
        {
            reward += 5.0; // "Sweet Spot": Wir nutzen fast exakt den verfügbaren Strom
        }

        // Q-Table Update mit der Bellman-Gleichung (vereinfacht)
        // Alpha ist deine Lernrate (z.B. 0.1)
        // Q[ts][ps][action] += alpha * (reward - Q[ts][ps][action]);
    }
    LOG_INFO(TAG_PID, "EXIT ML Task: %s", pcTaskGetTaskName(NULL));
}

/*
****** aPPLY
*/

void PinManager::apply(LogEntry &logEntry, double targetPower)
{
    unsigned long now = millis();
    LOG_INFO(TAG_PID, "PinManager::apply() - ENTER Task %s", pcTaskGetName(NULL));
   
    // 🔥 HARD STOP
    if (targetPower < 0.1)
    {
        digitalWrite(pinL1, LOW);
        digitalWrite(pinL2, LOW);
        analogWrite(pwmPin, 0);

        logEntry.state = 0;
        logEntry.pwm = 0;
        logEntry.power = 0;
        return;
    }

    // 🔹 RELAIS UPDATE ENTSCHEIDUNG
    bool relayUpdate = abs(targetPower - lastTargetPower) > DEAD_BAND;

    if (relayUpdate)
        lastTargetPower = targetPower;

    int currentActive = (digitalRead(pinL1) == HIGH ? 1 : 0) +
                        (digitalRead(pinL2) == HIGH ? 1 : 0);

    int desiredPhases = currentActive;

    // 🔥 HYSTERESE
    if (targetPower > onePhase * 1.2)
        desiredPhases = 1;
    if (targetPower > onePhase * 2.2)
        desiredPhases = 2;
    if (targetPower < onePhase * 0.8)
        desiredPhases = 0;
    if (targetPower < onePhase * 1.8 && currentActive == 2)
        desiredPhases = 1;

    // 🔹 RELAIS SCHALTEN
    if (relayUpdate && desiredPhases != currentActive && (now - lastSwitch > MIN_SWITCH))
    {
        digitalWrite(pinL1, desiredPhases >= 1);
        digitalWrite(pinL2, desiredPhases >= 2);

        lastSwitch = now;
        currentActive = desiredPhases;

        LOG_INFO(TAG_PID, "Relais → %d Phasen", desiredPhases);
    }

    // 🔹 RESTLEISTUNG
    double activeRelayPower = currentActive * onePhase;
    double rest = targetPower - activeRelayPower;

    if (rest < 0)
        rest = 0;
    if (rest > onePhase)
        rest = onePhase;

    double pwmVal = (rest / onePhase) * OUTPUT_MAX;

    // 🔥 GLÄTTUNG
    currentPWM = 0.7 * currentPWM + 0.3 * pwmVal;

    analogWrite(pwmPin, (int)currentPWM);

    // 🔹 LOGGING
    logEntry.state = logEntry.state = (digitalRead(pinL1) == HIGH) || (digitalRead(pinL2) == HIGH);
    logEntry.pwm = (int)currentPWM;
    

    LOG_INFO(TAG_PID, "PinManager::apply() - EXIT Task %s", pcTaskGetName(NULL));
}
/*
******* HELPER
*/
double PinManager::heaterPower()
{
    double p = 0;

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
double PinManager::getMeanOfAvailAblePower()
{
    // 1. Fensterverwaltung: Ältesten Wert entfernen, wenn Puffer voll
    if (availablePower.size() > MAX_LEN_MEASURE)
    {
        availablePower.erase(availablePower.begin());
    }

    size_t n = availablePower.size();

    // 2. Fallback: Zu wenig Daten für Ausreißer-Bereinigung
    if (n < 3)
    {
        if (n == 0)
            return 0.0;
        double sum = 0;
        for (double v : availablePower)
            sum += v;
        return sum / n;
    }

    // 3. Kopie erstellen und sortieren (wir wollen das Originalfenster nicht zerstören)
    std::vector<double> sortedValues = availablePower;
    std::sort(sortedValues.begin(), sortedValues.end());

    // 4. Trimmed Mean: Ersten und letzten Wert ignorieren
    double sum = 0;
    for (size_t i = 1; i < n - 1; i++)
    {
        sum += sortedValues[i];
    }

    // Division durch (n - 2), da wir zwei Werte entfernt haben
    return sum / (n - 2);
}