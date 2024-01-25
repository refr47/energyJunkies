#include "PID_v1.h"
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
}

void PinManager::config(Setup &setup, int digOut1, int digOut2, int anOut)
{

    onePhase = setup.heizstab_leistung_in_watt / 3.00;
    mOuts[id_DIG_PIN_1].init(digOut1, setup.pid_min_time_for_dig_output_inMS, Digital);
    mOuts[id_DIG_PIN_2].init(digOut2, setup.pid_min_time_for_dig_output_inMS, Digital);
    mOuts[id_ANA_PWM].init(anOut, setup.pid_min_time_for_dig_output_inMS, Analog);

    mDelayDigOutOn = millis();
    mDelayDigOutOff = millis();
}
void PinManager::reset()
{
    mOuts[id_ANA_PWM].setValue(0);
    for (int i = id_DIG_PIN_2; i >= id_DIG_PIN_1; i--)
    {
        if (mOuts[i].isDigOn())
        {
            mOuts[i].setValue(0);
            mDelayDigOutOff = millis();
        }
    }
}


#ifdef TEST_PID_WWWW
Setup d;
double prevAvailableWatt = 0.0;
double firstAvailableWatt=0.0;

#endif

bool PinManager::task(WEBSOCK_DATA &webSockData)
{
    bool result = false;

    /*  if (METER_DATA.acCurrentPower > 0.0) {
         DBGf("Energy Import %.3f",METER_DATA.acCurrentPower);
         return true;

     } */
    if (webSockData.states.froniusAPI)
    {
        availableWatt = FRONIUS.p_pv + FRONIUS.p_akku + FRONIUS.p_load;
        gridWatt = FRONIUS.p_grid;
        DBGf("pidManager::task,fronisAPI: availableWatt: %.3f, gridWatt: %.3f", availableWatt, gridWatt);
        DBGf("pidManager::task,fronisAPI: p_pv: %.3f, p_akku: %.3f, p_load: %.3f", FRONIUS.p_pv, FRONIUS.p_akku, FRONIUS.p_load);
    }
    else
    {
        availableWatt = INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower;
        gridWatt = INVERTER_DATA.acCurrentPower;
        DBGf("pidManager::task,modbus: availableWatt: %.3f, gridWatt: %.3f", availableWatt, gridWatt);
        DBGf("pidManager::task,modbus: acCurrentPower: %.3f, acCurrentPower: %.3f", INVERTER_DATA.acCurrentPower, METER_DATA.acCurrentPower);
    }

#ifdef TEST_PID_WWWW
    firstAvailableWatt=availableWatt;
    eprom_getSetup(d);
    // eprom_show(d);
    if (eprom_stammDataUpdate())
    {

        availableWatt = (double)d.exportWatt;
        DBGf("main PID-TEST update - available watts: %f", availableWatt);
        eprom_stammDataUpdateReset();
    }
    else
        availableWatt = prevAvailableWatt;
#endif
    // DBGf("PinManager::task: AvailableWatt: %.3f, gridWatt: %.3f", availableWatt, gridWatt);
    /* if (availableWatt < 10)
        return true; */
    if (abs(availableWatt) < 20.0)
    {
        return true;
    }

    if (availableWatt < 0.0)
    {
        availableWatt *= -1.0; // simpler for computing
        if (availableWatt < onePhase)
        {
            mAnalogOut = OUTPUT_MAX * availableWatt / onePhase;
            availableWatt = 0.0;
            DBGf("PidManager::task - reduce pwm %.3f", mAnalogOut);
            mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
            return true;
        }

        for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2 && availableWatt > onePhase; i++)
        {
            if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
            {
                mOuts[i].setValue(0.00);
                availableWatt -= onePhase;
                DBGf("PidManager:: availableWatt < 0 , Power Off DigiOut:%d", i);
            }
        }
        if (availableWatt <= onePhase)
        {
            mAnalogOut = OUTPUT_MAX * availableWatt / onePhase;
            mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
            DBGf("availableWatt < 0,availableWatt <= onePhase PWM: %f", mAnalogOut);
        }
        else
        {
            mOuts[id_ANA_PWM].setValue(0.00); // [0..255]
            mAnalogOut = 0.00;
            DBGf("availableWatt < 0,mCurrentPower <= onePhase PWM: %d", 0);
        }
    }
    else
    {
        if (availableWatt <= onePhase)
        {

            mAnalogOut = OUTPUT_MAX * availableWatt / onePhase;
            mOuts[id_ANA_PWM].setValue(mAnalogOut); // [0..255]
            availableWatt = 0.0;
            DBGf("PidManager:: <  0,mCurrentPower <= onePhase: %f", mAnalogOut);
        }
        else
        {

            for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2 && availableWatt > onePhase; i++)
            {
                if (!mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
                {
                    mOuts[i].setValue(1);
                    availableWatt -= onePhase;
                    DBGf("PidManager:: >  0,mCurrentPower >= onePhase: L %d active", i);
                }
            }
            if (availableWatt <= onePhase)
            {
                DBGf("PidManager:: >  0,mCurrentPower <= onePhas: pwm: %f", OUTPUT_MAX * availableWatt / onePhase);
                mOuts[id_ANA_PWM].setValue(OUTPUT_MAX * availableWatt / onePhase); // [0..255]
            }
            else
            {
                mOuts[id_ANA_PWM].setValue(OUTPUT_MAX); // [0..255]
                DBGf("PidManager:: >  0,mCurrentPower <= onePhas: pwm: %f", OUTPUT_MAX);
            }
        }
    }

    DBGf("PIDManagerTask::exit  mCurrPower (W): %f, anaOutput(PWM) %f, Dig1: %d, Dig2: %d", availableWatt, mAnalogOut, this->getStateOfDigPin(0), this->getStateOfDigPin(1));

#ifdef TEST_PID_WWWW
#ifdef INFLUX
    influx_write_test(firstAvailableWatt, availableWatt, webSockData);
#endif
    prevAvailableWatt = (int)availableWatt;

#endif
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