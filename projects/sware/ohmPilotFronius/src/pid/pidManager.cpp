// #include "PID_v1.h"
#include "esp32-hal.h"
#include "debugConsole.h"
#include "pidManager.h"
#include "mqtt.h"
#ifdef TEST_PID_WWWW
#include "eprom.h"
#endif
#ifdef INFLUX
#include "influx.h"
#endif

// pid settings and gains
#define OUTPUT_MIN 0
#define OUTPUT_MAX 255

#define ABS(N) ((N < 0) ? (-N) : (N))

#define aggKp 4
#define aggKi 0.2
#define aggKd 1

#define consKp 1
#define consKi 0.05
#define consKd 0.25

#define KP 1
#define KI 0.5
#define KD 0

// power has to be above set point for this time, before next digital output
// is activated (in ms)
#define DELAY_DIG_OUT_ON 5000
// delay turning of next digital output for this time (in ms)
#define DELAY_DIG_OUT_OFF 2000

// minimal on time for digital output (in ms)
static int MIN_ON_TIME = 10000;
// target power - max available power
// //static int TARGET_POWER = 5950;

static const int id_DIG_PIN_1 = 0;
static const int id_DIG_PIN_2 = 1;
static const int id_ANA_PWM = 2;

PinManager::PinManager()
{
    availablePower.clear();

    powerIndex = 0;
    storage = 0;
    boilerSwitchExternalOn = 0;
    testForBoilerSwitch = 0;
}

void PinManager::config(Setup &setup, int digOut1, int digOut2, int anOut)
{

    onePhase = setup.heizstab_leistung_in_watt / 3.00;
    mOuts[id_DIG_PIN_1].init(digOut1, MIN_ON_TIME, Digital);
    mOuts[id_DIG_PIN_2].init(digOut2, MIN_ON_TIME, Digital);
    mOuts[id_ANA_PWM].init(anOut, MIN_ON_TIME, Analog);

    mDelayDigOutOn = millis();
    mDelayDigOutOff = millis();
    boilerSwitchExternalOn = 0;
    testForBoilerSwitch = 0;
}

void PinManager::reset()
{
    mAnalogOut = 0.0;
    mOuts[id_ANA_PWM].setValue(mAnalogOut);
    for (int i = id_DIG_PIN_2; i >= id_DIG_PIN_1; i--)
    {
        if (mOuts[i].isDigOn())
        {
            mOuts[i].setValue(0);
            mDelayDigOutOff = millis();
        }
    }
    storage = 0.0;
}
void PinManager::switchOnL1()
{

    mOuts[id_DIG_PIN_1].setValue(1);
    mAnalogOut = 0.0;
    mOuts[id_ANA_PWM].setValue(mAnalogOut);
    storage += onePhase;
}
void PinManager::switchOnL2()
{

    mOuts[id_DIG_PIN_2].setValue(1);
    mAnalogOut = 0.0;
    mOuts[id_ANA_PWM].setValue(mAnalogOut);
    storage += onePhase;
}
void PinManager::switchOnL3()
{
    mAnalogOut = OUTPUT_MAX;
    mOuts[id_ANA_PWM].setValue(mAnalogOut);
    storage += onePhase;
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

#ifdef TEST_PID_WWWW
#define START_VALUE_FOR_AIVALABLE_WATT -99999.0
static Setup d;
// static double prevAvailableWatt = 0.0;
static double statusBoilerWatt = 0.0;
static double firstAvailableWatt = 0.0;
static double prevAvailableWatt = 0.0;
static double prozent = 0.0;
static boolean isNegative = false;
#endif

double PinManager::getWattBoundInRelays()
{

    double store = 0.0;
    DBGf("getWattBoundInRelays");
    for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2; i++)
    {
        /*     DBGf("index: %d,isDigOn(): %d, tivationTimeElapsed(): %d", i, mOuts[i].isDigOn(), mOuts[i].hasActivationTimeElapsed()); */
        if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
        {
            store += onePhase;
        }
        // DBGf("getWattBoundInRelays store: %f", store);
    }
    return store;
}

double PinManager::reduceRelayStorage()
{
    DBGf("PidManager:reduceRelayStorage , storage: %.3f BEGIN", storage);
    for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2; i++)
    {
        if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
        {
            mOuts[i].setValue(0);
            if (availableWatt - onePhase > 0.0)
            {
                availableWatt -= onePhase;
            }
            else
                availableWatt = 0.0;
            storage -= onePhase;
            DBGf("PidManager:: reduceRelayStorage < 0 , Power Off DigiOut:%d, availableWatt %.3f", i, availableWatt);
            break;
        }
    }
    DBGf("PidManager:reduceRelayStorage storage: %.3f EXIT", storage);
    return storage;
}

double PinManager::addRelayStorage()
{
    DBGf("PidManager:addRelayStorage storage: %.3f BEGIN", storage);
    for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2; i++)
    {
        if (!mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
        {
            mOuts[i].setValue(1);
            availableWatt -= onePhase;
            storage += onePhase;
            DBGf("PidManager:: >  0,addRelayStorage >= onePhase: L %d active", i);
            // return storage;
            break;
        }
    }
    DBGf("PidManager:addRelayStorage storage: %.3f ExIT", storage);
    return storage;
}

void PinManager::adustPWM()
{

    if (mAnalogOut >= OUTPUT_MAX)
    {
        DBGf("adustManager, nothing to do, mAnalogOut: %f", mAnalogOut);
    }
    if (availableWatt < onePhase)
    {
        DBGf(">  0,mCurrentPower <= onePhas: pwm: %f", OUTPUT_MAX * availableWatt / onePhase);
        mAnalogOut = OUTPUT_MAX * availableWatt / onePhase;
        mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
        availableWatt = 0.0;
    }
    else
    {
        mAnalogOut = OUTPUT_MAX;
        mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
        DBGf(">  0,mCurrentPower <= onePhas: pwm: %f", OUTPUT_MAX);
    }
}
bool PinManager::task(WEBSOCK_DATA &webSockData)
{
    /* bool result = false;
    static double pwm = 0.0; */

    DBGf("=================PID Start =======================");
    if (webSockData.states.froniusAPI)
    {
        if (storage > 0.0 || mAnalogOut > 0.0)
        {
            // p_load < 0 implies: consumtion of watt and if
            // storage+mAnalogOut + current consumption > 0: hardware bimetal
            // temperature of boiler switched off
            if (FRONIUS.p_load + storage + mAnalogOut > 0.0)
            {
                if (testForBoilerSwitch == 0)
                {
                    testForBoilerSwitch = millis();
                    DBGf("pidManager::task - test for external boiler switch started.");
                    boilerSwitchExternalOn = 1;
                    DBGf("pidManager::task - storage > current load (boiler thermostat has switch off) FRONIUS.p_load: %.3f, storage: %.3f, mAnalogOut: %.3f, Temperature: %.3f", FRONIUS.p_load, storage, mAnalogOut, webSockData.temperature.sensor1);
                    reset();
                }
                else
                {
                    ++boilerSwitchExternalOn;
                    storage = mAnalogOut = 0;
                    if (boilerSwitchExternalOn > 100)
                    {
                        DBGf("pidManager::task - boiler switch, counter > 100, timeSlot: %d", millis() - testForBoilerSwitch);
                        boilerSwitchExternalOn = 0;
                        testForBoilerSwitch = millis();
                    }
                }
                return true;
            }
            else
            {
                // testForBoilerSwitch = 0;
            }
        }

        if (FRONIUS.p_akku < 0.0)
        { // <0: laden, >0 entladen
            if (webSockData.setupData.externerSpeicherPriori == AKKU_PRIORITY_SUBORDINATED)
            {
                availableWatt = FRONIUS.p_akku + FRONIUS.p_grid; // gird < 0: einspeisen, > 0 bezug
                DBGf("PidManager::froniusAPI, akku priority subordinated (nachrangig), available Watt: %.3f", availableWatt);
            }
            else
            {
                DBGf("PidManager::froniusAPI, akku priority primary (vorrangig)available Watt: %.3f", availableWatt);
                availableWatt = FRONIUS.p_grid;
            }
        }
        else
            availableWatt = FRONIUS.p_grid;
        gridWatt = FRONIUS.p_grid;
        /*   DBGf("fronisAPI: availableWatt: %.3f, gridWatt: %.3f, store: %.3f", availableWatt, gridWatt, getWattBoundInRelays());
          DBGf("fronisAPI: p_pv: %.3f, p_akku: %.3f, p_load: %.3f", FRONIUS.p_pv, FRONIUS.p_akku, FRONIUS.p_load); */
#ifdef INFLUX
        influx_write_production(FRONIUS.p_pv, FRONIUS.p_akku, FRONIUS.p_load);
#endif
    }
    else
    {
        // METER_DATA.acCurrentPower < 0: export: else import
        availableWatt = INVERTER_DATA.acCurrentPower - METER_DATA.acCurrentPower;
        gridWatt = METER_DATA.acCurrentPower; //  <0:  einspeisen, >0: Bezug
                                              /* DBGf("modbus: availableWatt: %.3f, gridWatt: %.3f", availableWatt, gridWatt);
                                              DBGf("modbus: acCurrentPower: %.3f, acCurrentPower: %.3f", INVERTER_DATA.acCurrentPower, METER_DATA.acCurrentPower); */
#ifdef INFLUX
        influx_write_production(INVERTER_DATA.acCurrentPower, 0.0, METER_DATA.acCurrentPower);
#endif
    }

    if (webSockData.states.heating != HEATING_AUTOMATIC) // no pid controller, all is forced
    {
        DBGf("pidManager::task: heating: %d", webSockData.states.heating);
#ifdef INFLUX
        influx_write_test(storage, availableWatt, webSockData);
#endif

        return true; // nothing must be done due to overruling everything
    }

#ifdef TEST_PID_WWWW
    eprom_getSetup(d);
    // eprom_show(d);
    if (eprom_stammDataUpdate())
    {
        eprom_stammDataUpdateReset();
    }

    // availableWatt += (double)d.forceHeating;
    //  availableWatt -= statusBoilerWatt;
    availableWatt -= d.additionalLoad;
    firstAvailableWatt = availableWatt;
    DBGf("Abs: %f,  newAvailableWatt: %f, addLoad: %.3f", ABS(availableWatt), availableWatt, d.additionalLoad);

    /* if (prevAvailableWatt != START_VALUE_FOR_AIVALABLE_WATT)
        availableWatt = prevAvailableWatt; */
#endif

    if (powerIndex < MAX_LEN_MEASURE)
    {
        availablePower.push_back(availableWatt); // availablePower;
        ++powerIndex;
        // DBGf("sIZEOF BUFFER. %d", availablePower.size());
#ifdef TEST_PID_WWWW
#ifdef INFLUX
        influx_write_mean_val(availableWatt);
        return true;
#endif
#endif
        return true;
    }
    else
    {
        availableWatt = getMeanOfAvailAblePower();
        DBGf("Means of MAX_LEN_MEASURE-2 measures  %f", availableWatt);
        powerIndex = 0;
    }
#ifdef INFLUX
    influx_write_test(storage, availableWatt, webSockData);
#endif

    if (ABS(availableWatt) < 20.0)
    {
#ifdef TEST_PID_WWWW
        DBGf("ABS<20, PWM: %.3f", mAnalogOut);
#endif

        return true;
    }

    if (availableWatt > 0.0) // bezug!
    {
#ifdef TEST_PID_WWWW
        isNegative = true;
#endif

        DBGf("AvailableWatt: %f > 0, Consumation, analogOut: %.3f   ", availableWatt, mAnalogOut);

        storage = getWattBoundInRelays();
        DBGf("storage: %.3f , availableWatt: %.3f ", storage, availableWatt);
        if (storage > 0.0 && availableWatt >= onePhase)
        {
            DBGf("1 availableWatt < storage: %.3f", storage);
            /* if (storage >= onePhase)
            { */
            DBGf("2 storage %.3f - availableWatt %.3f> onePhase: ", storage, availableWatt);
            storage = reduceRelayStorage();
            DBGf("3 storage %.3f - availableWatt %.3f> onePhase: ", storage, availableWatt);
            // }
        }
        if (availableWatt >= onePhase)
        {
            DBGf("4 availableWatt >= onePhase %.3f", availableWatt);
            mAnalogOut = OUTPUT_MIN;
            mOuts[id_ANA_PWM].setValue(mAnalogOut);
            DBGf("5 availableWatt >= onePhase,pwm=0, analogOutput: %.3f", mAnalogOut);
        }
        else if (availableWatt < onePhase)
        {
            adustPWM();
            /* mAnalogOut = OUTPUT_MIN;
            mOuts[id_ANA_PWM].setValue(mAnalogOut); */
            DBGf("6 availableWatt < onePhase, mAnalogOut: %.3f", mAnalogOut);
        }

#ifdef HH
        if (availableWatt >= onePhase)
        {

            for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2 && availableWatt > onePhase; i++)
            {
                if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
                {
                    mOuts[i].setValue(0.00);
                    availableWatt -= onePhase;
                    statusBoilerWatt -= onePhase;
                    DBGf("PidManager:: availableWatt < 0 , Power Off DigiOut:%d", i);
                }
            }
        }
        if (availableWatt < onePhase && mAnalogOut > 0.00)
        {
            pwm = OUTPUT_MAX * availableWatt / onePhase;
            DBGf("new pwm: %.3f, mAnalogOut: %.3f ", pwm, mAnalogOut);
            prozent = 0.0;
            if (pwm <= mAnalogOut)
            {
                prozent = pwm / mAnalogOut;
                mAnalogOut -= pwm;
                statusBoilerWatt *= prozent;
            }
            else
            {
                DBGf("new pwm > mAnalogOut, mAnalogOut: %.3f, pwm: %.3f, REsET", mAnalogOut, pwm);
                mAnalogOut = 0.00;
                statusBoilerWatt = 0.00;
            }

            DBGf("PidManager::task - new pwm: %.3f, prozent: %.3f, reduce anaOut %.3f", pwm, prozent, mAnalogOut);
            mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
        }
        else if (mAnalogOut > 0.00)
        {
            mOuts[id_ANA_PWM].setValue(0.00); // [0..255]
            mAnalogOut = 0.00;
            availableWatt -= onePhase;
            statusBoilerWatt -= onePhase;
            DBGf("availableWatt < 0,mCurrentPower <= onePhase PWM: %d", 0);
        }
#endif
    }
    else // einspeisung, < 0!
    {
        availableWatt *= -1.0; // simpler for computing
        DBGf("AvilableWatt: %f > 0, production>Load, analogOut (PWM): %.3f", availableWatt, mAnalogOut);
#ifdef TEST_PID_WWWW
        isNegative = true;
#endif

        // storage = getWattBoundInRelays();
        /*   if (availableWatt < storage)
          {
              DBGf("1 availableWatt < storage: %.3f", storage);
              if (storage > 0.0)
              {
                  storage = reduceRelayStorage(storage);
                  DBGf("2 storage - availableWatt %.3f > onePhase %.3f", availableWatt, storage);
              }
          } */
        if (availableWatt > onePhase)
        {
            DBGf("3 storage - availableWatt - storage > onePhase %.3f > onePhase %.3f", availableWatt, storage);
            storage = addRelayStorage();
        }
        adustPWM();
#ifdef HH
        if (availableWatt <= onePhase)
        {

            mAnalogOut = OUTPUT_MAX * availableWatt / onePhase;
            mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
            statusBoilerWatt += availableWatt;
            availableWatt = 0.0;
            DBGf(">  0,mCurrentPower <= onePhase: %f", mAnalogOut);
        }
        else
        {

            for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2 && availableWatt >= onePhase; i++)
            {
                if (!mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
                {
                    mOuts[i].setValue(1);
                    availableWatt -= onePhase;
                    statusBoilerWatt += onePhase;
                    DBGf("PidManager:: >  0,mCurrentPower >= onePhase: L %d active", i);
                }
            }
            if (availableWatt < onePhase && mAnalogOut < 254)
            {
                DBGf(">  0,mCurrentPower <= onePhas: pwm: %f", OUTPUT_MAX * availableWatt / onePhase);
                statusBoilerWatt += availableWatt;
                mOuts[id_ANA_PWM].setValue(OUTPUT_MAX * availableWatt / onePhase); // [0..255]
                availableWatt = 0.0;
            }
            else if (mAnalogOut < 254)
            {
                mAnalogOut = OUTPUT_MAX;
                mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
                availableWatt -= onePhase;
                statusBoilerWatt += onePhase;

                DBGf(">  0,mCurrentPower <= onePhas: pwm: %f", OUTPUT_MAX);
            }
        }
#endif
    }
    /* if (isNegative)
        availableWatt *= -1.0; */

    DBGf("Eexit  mCurrPower (W): %.3f, storage: %.3f PWM: %.3f, Storage: %f, Dig1: %d, Dig2: %d", availableWatt, storage, mAnalogOut, getWattBoundInRelays(), this->getStateOfDigPin(0), this->getStateOfDigPin(1));

#ifdef TEST_PID_WWWW
#ifdef INFLUX
    influx_write_test(storage, availableWatt, webSockData);
#endif

    DBGf("Available: %.3f, storage: %.3f", availableWatt, storage);
    DBGf("=================PID End =======================");
#endif
#ifdef TEST_PID_WWWW
    prevAvailableWatt = firstAvailableWatt;
#endif
    delay(15000);
    return true;
}

#ifdef IIIIIIII

if (currentAvailablePower < 0.0)
{
    // Einspeisung
    mCurrentPower = currentAvailablePower * -1.00;
    if (mCurrentPower <= (double)setup.phasen_leistung_in_watt)
    {
        // leistung für 1 phase
        DBGf("PID Manager:: %f steht zur Verfügung", mCurrentPower);
        result = mPid.Compute();
        DBGf("PID Manager - leistung für 1 phase per pwm: %f", mAnalogOut);
        if (result)
            DBGf("PID Manager: Recalculated value, Analog Out: %f, MidSetPoint: %f", mAnalogOut, mPidSetPoint);
        else
            DBGf("PID Manager: Nothing happened");
        mOuts[id_ANA_PWM].setValue(mAnalogOut);
    }
    else
    {
        // leistung für eine volle phase (relay) + pwm

        // turn on digital output if power suffices
        for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2; i++)
        {
            if (!(mOuts[i].isDigOn() || mOuts[i].hasActivationTimeElapsed()))
            {
                mOuts[i].setValue(1);
                mCurrentPower -= (double)setup.phasen_leistung_in_watt;
                mPid.Compute();
                mOuts[id_ANA_PWM].setValue(mAnalogOut);
                DBGf("PID MAnager: Switched on Relais: %d, PWM: %f", i, mAnalogOut);
                break; // do not turn on the next output
            }
        }
    }
}
else
{
    mCurrentPower = currentAvailablePower;
    if (mCurrentPower <= (double)setup.phasen_leistung_in_watt)
    {
        // sache für pid, da schon strom bezogen werden muss (im unteren bereich)
        double prev_mAnalogOut = mAnalogOut;
        mPid.Compute();
        mAnalogOut = (prev_mAnalogOut + mAnalogOut) / 2.0;
        DBGf("PID Manager im Bezugsmodus (%f), previPWM %f calculatedPWM %f gemittelt %f", mCurrentPower, prev_mAnalogOut, mAnalogOut, (prev_mAnalogOut + mAnalogOut) / 2.0);
        mOuts[id_ANA_PWM].setValue(mAnalogOut);
    }
    else
    {
        for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2; i++)
        {
            if ((mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed()))
            {
                mOuts[i].setValue(1);
                mCurrentPower -= (double)setup.phasen_leistung_in_watt;
                if (mCurrentPower > (double)setup.phasen_leistung_in_watt)
                {
                    DBGf("PID Manager Switched Of relais: %d - still remains to much bezugspower: %f , set pwm to 0", i, mCurrentPower);
                    mAnalogOut = 0.0;
                }
                else
                {
                    mPid.Compute();
                    DBGf("PID Manager Switched Of relais: %d - remains power: %f , set pwm to %f", i, mCurrentPower, mAnalogOut);
                }
                mOuts[id_ANA_PWM].setValue(mAnalogOut);
                break; // do not turn on the next output
            }
        }
    }
}
#endif

#ifdef II
mCurrentPower = currentAvailablePower; // necessary ??
DBGf("PID Manager:: current available power: %f", mCurrentPower);
// mCurrentPower = pidContainer.mCurrentPower;
/*     double gap = abs(mPidSetPoint - mCurrentPower); // distance away from setpoint
    if (gap < 10)
    { // we're close to setpoint, use conservative tuning parameters
        mPid.SetTunings(consKp, consKi, consKd);
    }
    else
    {
        // we're far from setpoint, use aggressive tuning parameters
        mPid.SetTunings(aggKp, aggKi, aggKd);
    } */
result = mPid.Compute();
if (result)
{
    DBGf("PID Manager: Recalculated value, Analog Out: %f, MidSetPoint: %f", mAnalogOut, mPidSetPoint);
}
else
{
    DBGf("PID Manager: Nothing happened");
}

mOuts[id_ANA_PWM].setValue(mAnalogOut);

// turn on digital output if power suffices
if (mCurrentPower > mPidSetPoint && mAnalogOut > OUTPUT_MAX - 1)
{
    if (millis() - mDelayDigOutOn > setup.pid_min_time_without_contoller_inMS)
    {
        for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2; i++)
        {
            if (!mOuts[i].isDigOn())
            {
                mOuts[i].setValue(1);
                mDelayDigOutOn = millis(); // delay next output
                break;                     // do not turn on the next output
            }
        }
    }
}
else
{
    mDelayDigOutOn = millis();
}
// turn off digital output if power is low
DBGf("PID Manager: Condition 1: %d, Cond 2: %d Cond3: %d", mCurrentPower < mPidSetPoint, mAnalogOut<OUTPUT_MIN + 1, mDelayDigOutOff> setup.pid_min_time_before_switch_off_channel_inMS);

if (mCurrentPower < mPidSetPoint && mAnalogOut < OUTPUT_MIN + 1 && millis() - mDelayDigOutOff > setup.pid_min_time_before_switch_off_channel_inMS)
{
    for (int i = id_DIG_PIN_2; i >= id_DIG_PIN_1; i--)
    {
        if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
        {
            mOuts[i].setValue(0);
            mDelayDigOutOff = millis();
            break; // do not turn off the next output
        }
    }
}

// dbg("power:");
// dbg(power);
DBGf("PID  Manager  mCurrPower (W): %f, anaOutput(PWM) %f, mPidSetPoint: %f, Dig1: %d, Dig2: %d", mCurrentPower, mAnalogOut, mPidSetPoint, this->getStateOfDigPin(0), this->getStateOfDigPin(1));
/*   pidContainer.mCurrentPower=mCurrentPower;
  pidContainer.mAnalogOut=mAnalogOut;
  pidContainer.powerNotUseable=mPidSetPoint;
  pidContainer.PID_PIN1 = mOuts[id_DIG_PIN_1].isDigOn();
  pidContainer.PID_PIN2 = mOuts[id_DIG_PIN_2].isDigOn(); */
#endif

#ifdef IIIIIIIIIII

// DBGf("PID Params p: %f , i: %f,  d: %f", setup.pid_p, setup.pid_i, setup.pid_d);
mPid.SetTunings(setup.pid_p, setup.pid_i, setup.pid_d);

// setup.pidChanged = false;

mCurrentPower = *currentAvailablePower < 0.0 ? *currentAvailablePower * -1.0 : setup.pid_powerWhichNeedNotConsumed;
mPid.Compute();
// DBGf("PID Manager:: %f steht zur Verfügung mit midsetPoint: %f", mCurrentPower, mPidSetPoint);
/*     if (result)
        DBGf("PID Manager: Recalculated value, Analog Out: %f.", mAnalogOut);
    else
        DBGf("PID Manager: Nothing happened"); */

mOuts[id_ANA_PWM].setValue(mAnalogOut);
if (*currentAvailablePower < 0.0)
{
    consumedPower = ((mAnalogOut - analogOutPrev) / 255.0) * setup.phasen_leistung_in_watt;
    *currentAvailablePower += consumedPower;
    // consumedPower = ((mAnalogOut - analogOutPrev) / 255.0) * setup.phasen_leistung_in_watt;
    /* DBGf("PID Manager: EINSPEIS anaOutPref: %f, anaOut: %f consumedPower: %f after pwm remains: %f", analogOutPrev, mAnalogOut, consumedPower, *currentAvailablePower); */
#ifdef MQTT
    mqtt_publish_en(mAnalogOut, mCurrentPower);
#endif
}
else
{
    consumedPower = ((analogOutPrev - mAnalogOut) / 255.0) * setup.phasen_leistung_in_watt;
    *currentAvailablePower -= consumedPower;
    /* DBGf("PID Manager: BEZUG anaOutPref: %f, anaOut: %f consumedPower: %f after pwm remains: %f", analogOutPrev, mAnalogOut, consumedPower, *currentAvailablePower); */
#ifdef MQTT
    mqtt_publish_en(mAnalogOut, mCurrentPower);
#endif
}
// delay(4000);

// turn on digital output if power suffices
if (mCurrentPower > mPidSetPoint && mAnalogOut > OUTPUT_MAX - 1)
{
    if (millis() - mDelayDigOutOn > DELAY_DIG_OUT_ON)
    {
        for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2; i++)
        {
            DBGf("PID Manager: Search free digital output pin : %d", i);
            if (!mOuts[i].isDigOn())
            {
                DBGf("PID Manager: found : %d", i);
                mOuts[i].setValue(1);
                mDelayDigOutOn = millis(); // delay next output
                *currentAvailablePower += (double)setup.phasen_leistung_in_watt;
                break; // do not turn on the next output
            }
        }
    }
}
else
{
    mDelayDigOutOn = millis();
    // DBGf("PID Manager: time delay ");
}

/*   double gap = abs(mPidSetPoint - mCurrentPower); // distance away from setpoint
  if (gap < 10)
  { // we're close to setpoint, use conservative tuning parameters
      mPid.SetTunings(consKp, consKi, consKd);
  }
  else
  {
      // we're far from setpoint, use aggressive tuning parameters
      mPid.SetTunings(aggKp, aggKi, aggKd);
  }
  */
mPid.SetTunings(aggKp, aggKi, aggKd);
// turn off digital output if power is low
if (mCurrentPower < mPidSetPoint && mAnalogOut < OUTPUT_MIN + 1 && millis() - mDelayDigOutOff > DELAY_DIG_OUT_OFF)
{
    for (int i = id_DIG_PIN_2; i >= id_DIG_PIN_1; i--)
    {
        if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
        {
            DBGf("PID  Manager  mCurrPower (W): %f, anaOutput(PWM) %f, mPidSetPoint: %f, Dig1: %d, Dig2: %d", mCurrentPower, mAnalogOut, mPidSetPoint, this->getStateOfDigPin(0), this->getStateOfDigPin(1));
            mOuts[i].setValue(0);
            mDelayDigOutOff = millis();
            *currentAvailablePower -= (double)setup.phasen_leistung_in_watt;
            break; // do not turn off the next output
        }
    }
}
/*     DBGf("PID  Manager  mCurrPower (W): %f, anaOutput(PWM) %f, mPidSetPoint: %f, Dig1: %d, Dig2: %d", mCurrentPower, mAnalogOut, mPidSetPoint, this->getStateOfDigPin(0), this->getStateOfDigPin(1)); */

#endif

#ifdef IIIII

bool PinManager::task(Setup &setup, double currentP, TEMPERATURE &tempContainer)
{
    mCurrentPower = currentP + setup.pid_powerWhichNeedNotConsumed;
    double onePhase = setup.heizstab_leistung_in_watt / 3.00;

    if (mCurrentPower < 0.00)
    {
        mCurrentPower *= -1.00;
        DBGf("PidManager:: <  0,onePhase: %f", onePhase);
        if (mCurrentPower <= onePhase)
        {
            mAnalogOut = OUTPUT_MAX * mCurrentPower / onePhase;
            mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
            DBGf("PidManager:: <  0,mCurrentPower <= onePhase: %f", mAnalogOut);
        }
        else
        {

            for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2 && mCurrentPower > onePhase; i++)
            {
                if (!mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
                {
                    mOuts[i].setValue(1);
                    mCurrentPower -= onePhase;
                    DBGf("PidManager:: <  0,mCurrentPower >= onePhase: L %d active", i);
                }
            }
            if (mCurrentPower <= onePhase)
            {
                DBGf("PidManager:: <  0,mCurrentPower <= onePhas: pwm: %f", OUTPUT_MAX * mCurrentPower / onePhase);
                mOuts[id_ANA_PWM].setValue(OUTPUT_MAX * mCurrentPower / onePhase); // [0..255]
            }
            else
            {
                mOuts[id_ANA_PWM].setValue(OUTPUT_MAX); // [0..255]
                DBGf("PidManager:: <  0,mCurrentPower <= onePhas: pwm: %f", OUTPUT_MAX);
            }
        }
    }
    else // mCurrentPower>0, bezug
    {
        int curSensorTmp = (int)trunc((tempContainer.sensor1 + tempContainer.sensor2) / 2.00 + 0.5);
        DBGf("PidManager:: > 0, currSensorTemp: %d", curSensorTmp);
        if (curSensorTmp < setup.tempMinInGrad)
        {
            DBGf("MindesTemp unterschritten %d", setup.tempMinInGrad);
            mAnalogOut = OUTPUT_MAX;
            mOuts[id_ANA_PWM].setValue(OUTPUT_MAX); // [0..255]
            return true;
        }
        DBGf("PidManager:: > 0,onePhase: %f", onePhase);
        if (mCurrentPower <= onePhase) // Bezug < onePhase benötigt
        {
            mAnalogOut = OUTPUT_MAX * mCurrentPower / onePhase;
            mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
            DBGf("PidManager:: > 0,PWM: %f", mAnalogOut);
        }
        else // mCurrentPower > onePhase
        {
            for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2 && mCurrentPower > onePhase; i++)
            {
                if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
                {
                    mOuts[i].setValue(0.00);
                    mCurrentPower -= onePhase;
                    DBGf("PidManager:: > 0, Power On DigiOut:%d", i);
                }
            }
            if (mCurrentPower <= onePhase)
            {
                mAnalogOut = OUTPUT_MAX * mCurrentPower / onePhase;
                mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
                DBGf("PidManager:: > 0,mCurrentPower <= onePhase PWM: %f", mAnalogOut);
            }
            else
            {
                mOuts[id_ANA_PWM].setValue(0.00); // [0..255]
                mAnalogOut = 0.00;
                DBGf("PidManager:: > 0,mCurrentPower <= onePhase PWM: %d", 0);
            }
        }
    }

    DBGf("PID  Manager  mCurrPower (W): %f, anaOutput(PWM) %f, Dig1: %d, Dig2: %d", mCurrentPower, mAnalogOut, this->getStateOfDigPin(0), this->getStateOfDigPin(1));
    DBGf("PidManager --exit");
    delay(2000);
    return true;
}
#endif