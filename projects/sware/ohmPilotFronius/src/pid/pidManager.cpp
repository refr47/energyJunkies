#include "pinManager.h"

void PinManager::config(double totalPower, int l1, int l2, int pwm)
{
    onePhase = totalPower / 3.0;

    pinL1 = l1;
    pinL2 = l2;
    pwmPin = pwm;

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

bool PinManager::preCheck(WEBSOCK_DATA &webSockData, double temp,unsigned long nowMS)
{
    // SAFETY,
    LOG_DEBUG("PinManager::preCheck:");
   
    // LEGIONELLA
    if (nowMS - lastLegionella > webSockData.setupData.legionellenDelta /*7UL * 24 * 3600 * 1000*/)
        legionella = true;

    if (legionella)
    {
        if (temp >= webSockData.setupData.legionellenMaxTemp /*LEG_TEMP*/)
        {
            legionella = false;
            lastLegionella = nowMS;
            LOG_DEBUG("PinManager::preCheck: Legionellen Temperatur <%.3f> erreicht: <%.3f>", webSockData.setupData.legionellenMaxTemp, temp);
        }
        else
        {
            apply(onePhase * 3);
            powerIndex = 0;
            return true;
        }
    }

    // allowed boiler temp
    if (temp >= webSockData.setupData.tempMaxAllowedInGrad)
    {
        LOG_DEBUG("PinManager::preCheck: Max Temperatur <%.3f> erreicht: <%.3f>, abschalten", webSockData.setupData.tempMaxAllowedInGrad, temp);
        reset();
        powerIndex = 0;
        return true;
    }
    if (temp < webSockData.setupData.tempMinInGrad)
    {
        LOG_DEBUG("PinManager::preCheck: Min Temperatur <%.3f> erreicht: <%.3f>, einschalten", webSockData.setupData.tempMinInGrad, temp);
        apply(onePhase * 3);
        powerIndex = 0;
        return true;
    }

    if (webSockData.states.heating != HEATING_AUTOMATIC) // no pid controller, all is forced
    {
        LOG_DEBUG("PinManager::preCheck:  Manuelle Steuerung - keine Automatik");
        powerIndex = 0;
        return true; // nothing must be done due to overruling everything
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
                LOG_DEBUG("PinManager::preCheck (Fronius) Akku priority subordinated (nachrangig), available Watt: %.3f", availableWatt);
            }
            else
            {
                LOG_DEBUG("PinManager::preCheck (Fronius) Akku priority primary (vorrangig)available Watt: %.3f", availableWatt);
                availableWatt = webSockData.fronius_SOLAR_POWERFLOW.p_grid;
            }
        }
        else
        {
            availableWatt = webSockData.fronius_SOLAR_POWERFLOW.p_grid;
            LOG_DEBUG("PinManager::preCheck (Fronius) Available Watt: %.3f", availableWatt);
        }
    }
    else
    {
        // gwebSockData.mbContainer.meterValues.data.acCurrentPower < 0: export: else import
        availableWatt = webSockData.mbContainer.meterValues.data.acCurrentPower;
        LOG_DEBUG("PinManager::preCheck No froniusAPI) AvailableWatt: %.3f", availableWatt);
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
        return true;
    }
    else
    {

        LOG_INFO("PinManager::preCheck - Means of MAX_LEN_MEASURE-2 measures  %f", availableWatt);
        powerIndex = 0;
    }

    return false;
}

#define ABS(N) ((N < 0) ? (-N) : (N))

void PinManager::update(WEBSOCK_DATA &webSockData /*, double temp, int hour*/)
{
    unsigned long now = millis();
    double temp = (webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2.0;

    if (preCheck(webSockData, temp,now))
        return;                                       //
    double measuredPower = getMeanOfAvailAblePower(); // geglättete Werte
    if (ABS(measuredPower) < EPSILON_TEMP)
    {
        LOG_INFO("PinManager::task eXIT true, AvailableWatt: %.3f < 20.0", measuredPower);
        return ;
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
    double target = phases * onePhase + pwmPower;

    apply(target);
    /*
    Skalierte Strafe: Anstatt nur -2 zu geben, wenn Strom bezogen wird, bestrafst du hohen Bezug stärker. Das lehrt den Algorithmus, bei knapper PV-Leistung eher vorsichtig zu sein.

  Sweet Spot Bonus: Der Agent bekommt eine hohe Belohnung, wenn measuredPower nahe bei 0 liegt. Das ist das Ziel: Den Hausanschluss auf 0W zu halten.

  Vermeidung von Extremen: Wenn die Temperatur zu hoch wird, sinkt der Reward, sodass der Agent lernt, die Leistung rechtzeitig zu drosseln, bevor der preCheck (Sicherheit) hart abschaltet.
    */

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


/*
****** aPPLY
*/

void PinManager::apply(double targetPower)
{
    unsigned long now = millis();

    // 1. TOTZONE: Wenn die Änderung zum letzten Mal minimal ist,
    // überspringen wir die Relais-Prüfung (spart CPU und schont Logik).
    if (abs(targetPower - lastTargetPower) < DEAD_BAND)
    {
        // Wir aktualisieren nur das PWM, falls nötig, aber lassen die Relais in Ruhe
    }
    else
    {
        lastTargetPower = targetPower;
    }

    // 2. RELAIS-LOGIK (mit Hysterese & Zeit-Sperre)
    int desiredPhases = (int)(targetPower / onePhase);
    if (desiredPhases > 2)
        desiredPhases = 2;

    // Aktuellen Hardware-Zustand lesen
    int currentActive = (digitalRead(pinL1) ? 1 : 0) + (digitalRead(pinL2) ? 1 : 0);

    if (desiredPhases != currentActive && (now - lastSwitch > MIN_SWITCH))
    {
        // Nur schalten, wenn wir uns wirklich sicher sind
        digitalWrite(pinL1, desiredPhases >= 1);
        digitalWrite(pinL2, desiredPhases >= 2);
        lastSwitch = now;

        LOG_INFO("Relais-Update: %d Phasen aktiv (Soll: %.1f W)", desiredPhases, targetPower);
    }

    // 3. PWM-LOGIK (Der dynamische Ausgleich)
    // WICHTIG: Die PWM muss immer das ausgleichen, was die Relais gerade NICHT decken.
    double activeRelayPower = (digitalRead(pinL1) ? onePhase : 0) + (digitalRead(pinL2) ? onePhase : 0);
    double rest = targetPower - activeRelayPower;

    // Sicherheits-Begrenzung für PWM
    if (rest < 0)
        rest = 0;
    if (rest > onePhase)
        rest = onePhase;

    double pwmVal = (rest / onePhase) * OUTPUT_MAX;

    currentPWM = pwmVal;
    analogWrite(pwmPin, (int)pwmVal);
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
        return digitalRead(pinL1);
    else if (pin == 1)
        return digitalRead(pinL2);
    else
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
    analogWrite(pwmPin, OUTPUT_MAX);

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