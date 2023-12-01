#include "PID_v1.h"
#include "esp32-hal.h"
#include "debugConsole.h"
#include "pidManager.h"

// pid settings and gains
#define OUTPUT_MIN 0
#define OUTPUT_MAX 255
#ifdef PID_LIB
#define aggKp 4
#define aggKi 0.2
#define aggKd 1

#define consKp 1
#define consKi 0.05
#define consKd 0.25

#define KP 1
#define KI 0.5
#define KD 0
#endif

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

#ifdef PID_LIB
    : mPid(&mCurrentPower, &mAnalogOut, &mPidSetPoint, KP, KI, KD, REVERSE) // RESERVE
#endif
{
}
void PinManager::config(Setup &setup, int digOut1, int digOut2, int anOut)
{

    mPidSetPoint = setup.phasen_leistung_in_watt; // 1/3 for pid controller for pwm
    mOuts[id_DIG_PIN_1].init(digOut1, setup.pid_min_time_for_dig_output_inMS, Digital);
    mOuts[id_DIG_PIN_2].init(digOut2, setup.pid_min_time_for_dig_output_inMS, Digital);
    mOuts[id_ANA_PWM].init(anOut, setup.pid_min_time_for_dig_output_inMS, Analog);

    mDelayDigOutOn = millis();
    mDelayDigOutOff = millis();
    mPidSetPoint = setup.pid_powerWhichNeedNotConsumed; // 10 w
#ifdef PID_LIB
    mPid.SetMode(AUTOMATIC);
    // mPid.SetOutputLimits(OUTPUT_MIN, setup.phasen_leistung_in_watt);
    mPid.SetOutputLimits(OUTPUT_MIN, OUTPUT_MAX);
    // mPid.SetSampleTime(50); default sample time is 0,1 or 100
#endif
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
// currentP: < 0 : einspeisung, >0 Bezug

#ifdef PID_LIB
bool PinManager::task(Setup &setup, double currentAvailablePower, TEMPERATURE &container)
{
    bool result = false;

    // DBGf("PID Params p: %f , i: %f,  d: %f", setup.pid_p, setup.pid_i, setup.pid_d);
    mPid.SetTunings(setup.pid_p, setup.pid_i, setup.pid_d);

    setup.pidChanged = false;

    mCurrentPower = currentAvailablePower;
    mPid.Compute();
    // DBGf("PID Manager:: %f steht zur Verfügung mit midsetPoint: %f", mCurrentPower, mPidSetPoint);
    if (result)
        DBGf("PID Manager: Recalculated value, Analog Out: %f.", mAnalogOut);
    else
        DBGf("PID Manager: Nothing happened");
    mOuts[id_ANA_PWM].setValue(mAnalogOut);

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
                    break;                     // do not turn on the next output
                }
            }
        }
    }
    else
    {
        mDelayDigOutOn = millis();
        DBGf("PID Manager: time delay ");
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
                mOuts[i].setValue(0);
                mDelayDigOutOff = millis();
                break; // do not turn off the next output
            }
        }
    }
    DBGf("PID  Manager  mCurrPower (W): %f, anaOutput(PWM) %f, mPidSetPoint: %f, Dig1: %d, Dig2: %d", mCurrentPower, mAnalogOut, mPidSetPoint, this->getStateOfDigPin(0), this->getStateOfDigPin(1));
    return 0;
}
#endif

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