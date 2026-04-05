#include "pinManager.h"

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

    for (int i = 0; i < S_T; i++)
        for (int j = 0; j < S_P; j++)
            for (int k = 0; k < A; k++)
                Q[i][j][k] = 0;
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
int PinManager::chooseAction(int t, int pv)
{
    if (random(0, 100) < epsilon * 100)
        return random(0, A);

    int best = 0;
    double maxQ = Q[t][pv][0];

    for (int i = 1; i < A; i++)
        if (Q[t][pv][i] > maxQ)
        {
            maxQ = Q[t][pv][i];
            best = i;
        }

    return best;
}

double PinManager::actionFactor(int a)
{
    return a / 4.0;
}

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
    // LOG_DEBUG("PinManager::PID:");
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
            LOG_DEBUG(TAG_PID, "PID: Legionellen Temperatur <%s> erreicht: <%s>", fToStr(webSockData.setupData.legionellenMaxTemp, 5, 1, tempBuf), fToStr(temp,5, 1, tempBuf1));
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

        logEntry.state = digitalRead((pinL1) ? 1 : 0) + (digitalRead(pinL2) ? 1 : 0);
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
                char tempBuf[20];  // Platz für "-123.45\0"
                LOG_DEBUG(TAG_PID, "PID (Fronius) Akku priority subordinated (nachrangig), available Watt: %s", fToStr(availableWatt, 5, 1, tempBuf));
            }
            else
            {
                char tempBuf[20];  // Platz für "-123.45\0"
                LOG_DEBUG(TAG_PID, "PID (Fronius) Akku priority primary (vorrangig)available Watt: %s", fToStr(availableWatt, 5, 1, tempBuf));
                availableWatt = webSockData.fronius_SOLAR_POWERFLOW.p_grid;
            }
        }
        else
        {
            availableWatt = webSockData.fronius_SOLAR_POWERFLOW.p_grid;
                char tempBuf[20];  // Platz für "-123.45\0"
            LOG_DEBUG(TAG_PID, "PID (Fronius) Available Watt: %s", fToStr(availableWatt, 5, 1, tempBuf));
        }
    }
    else
    {
        // gwebSockData.mbContainer.meterValues.data.acCurrentPower < 0: export: else import
        availableWatt = webSockData.mbContainer.meterValues.data.acCurrentPower;
        char tempBuf[20];  // Platz für "-123.45\0"
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

void PinManager::update(WEBSOCK_DATA &webSockData /*, double temp, int hour*/)
{
    unsigned long now = millis();
    LogEntry logEntry;

    int temp = (webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2;
    logEntry.tag="PID";
    logEntry.temp = temp;
    logEntry.ts = now;
    ControlMode currentMode = preCheck(webSockData, temp, now, logEntry);
    double measuredPower = 0.0;
    double targetPower = 0.0;
    bool doML = true;
    switch (currentMode)
    {
    case MODE_OFF:
        targetPower = 0;
        measuredPower = 0;
        doML = false;
        reset(); // Interne Zähler zurücksetzen
        LOG_INFO(TAG_PID, "HEAT OFF (Sicherheit) - Alle Relais aus, PWM 0");
        break;

    case MODE_LEGIONELLA:
    case MODE_MIN_TEMP:
        targetPower = measuredPower = onePhase * 3; // Volle Kraft
        doML = false;
        LOG_INFO(TAG_PID, "HEAT ON (Sicherheit) - Alle Relais ein, PWM 254");
        break;

    case MODE_MANUAL:
        // Hier einfach den aktuellen Ist-Wert lassen oder aus webSockData lesen
        LOG_INFO(TAG_PID, "Manuelle Steuerung");
        doML = false;
        return;

    case MODE_AUTO:
        // Nur hier läuft deine RL-Logik!
        measuredPower = getMeanOfAvailAblePower();
        LOG_DEBUG(TAG_PID, "RL AUTO - Gemessene Leistung: %.3f W ", measuredPower);

        if (abs(measuredPower) < EPSILON_TEMP)
        {
            targetPower = 0; // <--- DAS schaltet aus, wenn kein Strom da ist!
        }
        else
        {
            if (ABS(measuredPower) < EPSILON_TEMP)
            {
                char tempBuf[20];  // Platz für "-123.45\0"
                char tempBuf1[30]; // Platz für "-123.45\0"
                LOG_INFO(TAG_PID, "PID eXIT true, AvailableWatt: %s < %s (Epsilon)", fToStr(measuredPower, 5, 1, tempBuf), fToStr(EPSILON_TEMP, 5, 1, tempBuf1));
                return;
            }
            double heater = heaterPower();
            double effective = (-measuredPower) + heater;

            // deterministic baseline
            // 🔢 physikalisch korrekt aufteilen
            int phases = (int)(effective / onePhase);
            if (phases > 2)
                phases = 2;

            double remaining = effective - phases * onePhase;

            // 🧠 RL beeinflusst NUR den Rest
            int ts = tempState(temp);
            int ps = pvState(measuredPower);

            int action = chooseAction(ts, ps);
            double factor = actionFactor(action);

            // 🎯 PWM-Anteil
            double pwmPower = remaining * factor;

            // 🔥 finale Leistung
            targetPower = phases * onePhase + pwmPower;
        }
        break;
    }

    apply(logEntry, targetPower);
    /*
    Skalierte Strafe: Anstatt nur -2 zu geben, wenn Strom bezogen wird, bestrafst du hohen Bezug stärker. Das lehrt den Algorithmus, bei knapper PV-Leistung eher vorsichtig zu sein.

  Sweet Spot Bonus: Der Agent bekommt eine hohe Belohnung, wenn measuredPower nahe bei 0 liegt. Das ist das Ziel: Den Hausanschluss auf 0W zu halten.

  Vermeidung von Extremen: Wenn die Temperatur zu hoch wird, sinkt der Reward, sodass der Agent lernt, die Leistung rechtzeitig zu drosseln, bevor der preCheck (Sicherheit) hart abschaltet.
    */
    int ts = tempState(temp);
    int ps = pvState(measuredPower);
    int action = chooseAction(ts, ps);

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
        Q[ts][ps][action] += alpha * (reward - Q[ts][ps][action]);
    }
}

/*
****** aPPLY
*/

void PinManager::apply(LogEntry &logEntry, double targetPower)
{
    unsigned long now = millis();
    bool onlyPWM = false;
    rest = 0;

    // 1. TOTZONE: Wenn die Änderung zum letzten Mal minimal ist,
    // überspringen wir die Relais-Prüfung (spart CPU und schont Logik).
    if (targetPower > 0.1 && abs(targetPower - lastTargetPower) < DEAD_BAND)
    {
        onlyPWM = true;
    }
    else
    {
        lastTargetPower = targetPower;
    }

    if (!onlyPWM)
    {
        // 2. RELAIS-LOGIK (mit Hysterese & Zeit-Sperre)
        int desiredPhases = (int)(targetPower / onePhase);
        if (desiredPhases > 2)
            desiredPhases = 2;
        // Bei 0 Watt IMMER sofort Phasen auf 0 setzen
        if (targetPower < 0.1)
            desiredPhases = 0;
        // Aktuellen Hardware-Zustand lesen
        int currentActive = (digitalRead(pinL1) ? 1 : 0) + (digitalRead(pinL2) ? 1 : 0);

        if (desiredPhases != currentActive && (now - lastSwitch > MIN_SWITCH))
        {
            // Nur schalten, wenn wir uns wirklich sicher sind
            digitalWrite(pinL1, desiredPhases >= 1);
            digitalWrite(pinL2, desiredPhases >= 2);
            lastSwitch = now;
            LOG_INFO(TAG_PID, "Relais geschaltet: %d Phasen", desiredPhases);

            // LOG_INFO("Relais-Update: %d Phasen aktiv (Soll: %.1f W)", desiredPhases, targetPower);
        }
        // 3. PWM-LOGIK (Der dynamische Ausgleich)
        // WICHTIG: Die PWM muss immer das ausgleichen, was die Relais gerade NICHT decken.
        double activeRelayPower = (digitalRead(pinL1) ? onePhase : 0) + (digitalRead(pinL2) ? onePhase : 0);

        rest = targetPower - activeRelayPower;
        LOG_DEBUG(TAG_PID, "PID L1: %d, L2: %d, Power: %.1f  W", digitalRead(pinL1), digitalRead(pinL2), rest);
    }

    // Sicherheits-Begrenzung für PWM
    if (rest < 0)
        rest = 0;
    if (rest > onePhase)
        rest = onePhase;

    double pwmVal = (rest / onePhase) * OUTPUT_MAX;

    currentPWM = pwmVal;
    analogWrite(pwmPin, (int)pwmVal);
   
    logEntry.state = digitalRead((pinL1) ? 1 : 0) + (digitalRead(pinL2) ? 1 : 0);
    logEntry.pwm = (int)currentPWM;
    logEntry.power = rest;
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