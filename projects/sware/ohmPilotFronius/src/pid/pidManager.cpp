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
// //static inboolt DELAY_DIG_OUT_ON = 5000;
// delay turning of next digital output for this time (in ms)
// //static int DELAY_DIG_OUT_OFF = 2000;

// minimal on time for digital output (in ms)
static int MIN_ON_TIME = 10000;
// target power - max available power
// //static int TARGET_POWER = 5950;

static const int id_DIG_PIN_1 = 0;
static const int id_DIG_PIN_2 = 1;
static const int id_ANA_PWM = 2;

PinManager::PinManager()
#ifdef PID_LIB
    : mPid(&mCurrentPower, &mAnalogOut, &mPidSetPoint, KP, KI, KD, REVERSE)
#endif
{

// mPidSetPoint = TARGET_POWER;
#ifdef PID_LIB
    mPid.SetMode(AUTOMATIC);
    mPid.SetOutputLimits(OUTPUT_MIN, OUTPUT_MAX);
    mPid.SetSampleTime(50);
#endif
}
void PinManager::config(Setup &setup, int digOut1, int digOut2, int anOut)
{

    mPidSetPoint = setup.pid_targetPowerInWatt;
    mOuts[id_DIG_PIN_1].init(digOut1, setup.pid_min_time_for_dig_output_inMS, Digital);
    mOuts[id_DIG_PIN_2].init(digOut2, setup.pid_min_time_for_dig_output_inMS, Digital);
    mOuts[id_ANA_PWM].init(anOut, setup.pid_min_time_for_dig_output_inMS, Analog);

    mDelayDigOutOn = millis();
    mDelayDigOutOff = millis();
}

// currentP: < 0 : einspeisung, >0 Bezug
bool PinManager::task(Setup &setup, double currentP, TEMPERATURE &tempContainer)
{
    mCurrentPower = currentP + setup.pid_targetPowerInWatt;
    double onePhase = setup.heizstab_leistung_in_watt / 3.00;

    if (mCurrentPower < 0.00)
    {
        mCurrentPower *= -1.00;

        if (mCurrentPower <= onePhase)
        {
            mOuts[id_ANA_PWM].setValue(OUTPUT_MAX * mCurrentPower / onePhase); // [0..255]
        }
        else
        {

            for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2 && mCurrentPower > onePhase; i++)
            {
                if (!mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
                {
                    mOuts[i].setValue(1);
                    mCurrentPower -= onePhase;
                }
            }
            if (mCurrentPower <= onePhase)
            {
                mOuts[id_ANA_PWM].setValue(OUTPUT_MAX * mCurrentPower / onePhase); // [0..255]
            }
            else
            {
                mOuts[id_ANA_PWM].setValue(OUTPUT_MAX); // [0..255]
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
            mOuts[id_ANA_PWM].setValue(OUTPUT_MAX); // [0..255]
            return true;
        }
        if (mCurrentPower <= onePhase) // Bezug < onePhase benötigt
        {
            mOuts[id_ANA_PWM].setValue(OUTPUT_MAX * mCurrentPower / onePhase); // [0..255]
        }
        else // mCurrentPower > onePhase
        {
            for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2 && mCurrentPower > onePhase; i++)
            {
                if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
                {
                    mOuts[i].setValue(0);
                    mCurrentPower -= onePhase;
                }
            }
            if (mCurrentPower <= onePhase)
            {
                mOuts[id_ANA_PWM].setValue(OUTPUT_MAX * mCurrentPower / onePhase); // [0..255]
            }
            else
            {
                mOuts[id_ANA_PWM].setValue(0); // [0..255]
            }
        }
        return true;
    }
}

#ifdef PID_LIB
int PinManager::task(Setup &setup, double currentP)
{
    bool result = false;

    if (setup.pidChanged)
    {
        this->config(setup);
        DBGf("PID Params changed / updated");
        setup.pidChanged = false;
    }
    mCurrentPower = currentP; // necessary ??
    // mCurrentPower = pidContainer.mCurrentPower;
    double gap = abs(mPidSetPoint - mCurrentPower); // distance away from setpoint
    if (gap < 10)
    { // we're close to setpoint, use conservative tuning parameters
        mPid.SetTunings(consKp, consKi, consKd);
    }
    else
    {
        // we're far from setpoint, use aggressive tuning parameters
        mPid.SetTunings(aggKp, aggKi, aggKd);
    }
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

    return 0;
}
#endif