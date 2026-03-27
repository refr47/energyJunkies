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
    double temp = (webSockData.temperature.sensor1 + webSockData.temperature.sensor1) / 2.0;

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
    double base = basePower(effective);

    // RL decision
    int ts = tempState(temp);
    int ps = pvState(measuredPower);

    int action = chooseAction(ts, ps);
    double factor = actionFactor(action);

    double target = base * factor;

    apply(target);

    // reward
    double reward = 0;

    if (temp >= 50 && temp <= 60)
        reward += 1;

    if (measuredPower > 0)
        reward -= 2;

    Q[ts][ps][action] += alpha * (reward - Q[ts][ps][action]);
}

/*
****** aPPLY
*/

void PinManager::apply(double power)
{
    int phases = (int)(power / onePhase);
    if (phases > 2)
        phases = 2;

    double rest = power - phases * onePhase;

    if (millis() - lastSwitch > MIN_SWITCH)
    {
        digitalWrite(pinL1, phases >= 1);
        digitalWrite(pinL2, phases >= 2);
        lastSwitch = millis();
    }

    double pwm = 0;
    if (rest > 0)
        pwm = OUTPUT_MAX * (rest / onePhase);

    currentPWM = pwm;
    analogWrite(pwmPin, (int)pwm);
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

    std::sort(availablePower.begin(), availablePower.end());
    double sum = 0;
    for (int jj = 1; jj < availablePower.size() - 1; jj++)
    {
        sum += availablePower[jj];
    }
    // DBGf("getMeanOfAvailablePower: %.3f, length: %d", sum, availablePower.size());

    double mean = static_cast<double>(sum) / (availablePower.size() - 2);
    // DBGf("getMeanOfAvailablePower: %.3f", mean);
    availablePower.clear();
    return mean;
}