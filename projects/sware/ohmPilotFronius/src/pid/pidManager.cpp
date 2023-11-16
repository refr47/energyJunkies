#include "PID_v1.h"
#include "esp32-hal.h"
#include "debugConsole.h"
#include "pidManager.h"

// pid settings and gains
#define OUTPUT_MIN 0
#define OUTPUT_MAX 255
#define KP 1
#define KI 0.5
#define KD 0

// power has to be above set point for this time, before next digital output
// is activated (in ms)
// //static int DELAY_DIG_OUT_ON = 5000;
// delay turning of next digital output for this time (in ms)
// //static int DELAY_DIG_OUT_OFF = 2000;

// minimal on time for digital output (in ms)
static int MIN_ON_TIME = 10000;
// target power - max available power
// //static int TARGET_POWER = 5950;

static const int id_DIG_PIN_1 = 0;
static const int id_DIG_PIN_2 = 1;
static const int id_ANA_PWM = 2;

PinManager::PinManager(int digOut1, int digOut2, int anOut)
    : mPid(&mCurrentPower, &mAnalogOut, &mPidSetPoint, KP, KI, KD, REVERSE)
{
    mOuts[id_DIG_PIN_1].init(digOut1, MIN_ON_TIME, Digital);
    mOuts[id_DIG_PIN_2].init(digOut2, MIN_ON_TIME, Digital);
    mOuts[id_ANA_PWM].init(anOut, MIN_ON_TIME, Analog);

    mDelayDigOutOn = millis();
    mDelayDigOutOff = millis();

    // mPidSetPoint = TARGET_POWER;
    mPid.SetMode(AUTOMATIC);
    mPid.SetOutputLimits(OUTPUT_MIN, OUTPUT_MAX);
    mPid.SetSampleTime(50);

}
void PinManager::config(Setup &setup)
{
    /*  DELAY_DIG_OUT_ON = setup.pid_min_time_without_contoller_inMS;
     DELAY_DIG_OUT_OFF = setup.pid_min_time_before_switch_off_channel_inMS;
     MIN_ON_TIME = setup.pid_min_time_for_dig_output_inMS;
     TARGET_POWER = setup.pid_targetPowerInWatt; */
    mPidSetPoint = setup.pid_targetPowerInWatt;
}
int PinManager::task(Setup &setup, double currentP)
{
    if (setup.pidChanged)
    {
        this->config(setup);
        DBGf("PID Params changed / updated");
        setup.pidChanged = false;
    }
    Serial.println(" PinManager::task with o2 params");
    //mCurrentPower = currentP;  // necessary ??
    //mCurrentPower = pidContainer.mCurrentPower;

    mPid.Compute();
    DBGf("--- 1");
    mOuts[id_ANA_PWM].setValue(mAnalogOut);
DBGf("--- 2");
    // turn on digital output if power suffices
    if (mCurrentPower > mPidSetPoint && mAnalogOut > OUTPUT_MAX - 1)
    {
        DBGf("--- 3");
        if (millis() - mDelayDigOutOn > setup.pid_min_time_without_contoller_inMS)
        {
            for (int i = id_DIG_PIN_1; i <= id_DIG_PIN_2; i++)
            {
                if (!mOuts[i].isDigOn())
                {
                    DBGf("--- 4");
                    mOuts[i].setValue(1);
                    mDelayDigOutOn = millis(); // delay next output
                    break;                     // do not turn on the next output
                }
            }
        }
    }
    else
    {
        DBGf("--- 5");
        mDelayDigOutOn = millis();
    }
    // turn off digital output if power is low
    if (mCurrentPower < mPidSetPoint && mAnalogOut < OUTPUT_MIN + 1 && millis() - mDelayDigOutOff > setup.pid_min_time_before_switch_off_channel_inMS)
    {
        DBGf("--- 6");
        for (int i = id_DIG_PIN_2; i >= id_DIG_PIN_1; i--)
        {
            if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed())
            {
                mOuts[i].setValue(0);
                mDelayDigOutOff = millis();
                DBGf("--- 7");
                break; // do not turn off the next output
            }
        }
    }

    // dbg("power:");
    // dbg(power);
    DBGf("  !!!!!!!! PID  Manager  mCurrPower (W): %f, anaOutput(PWM) %f, Einspeisung: %d, Dig1: %s, Dig2: %s", mCurrentPower, mAnalogOut, mPidSetPoint, mOuts[id_DIG_PIN_1].isDigOn(), mOuts[id_DIG_PIN_2].isDigOn());
  /*   pidContainer.mCurrentPower=mCurrentPower;
    pidContainer.mAnalogOut=mAnalogOut;
    pidContainer.powerNotUseable=mPidSetPoint;
    pidContainer.PID_PIN1 = mOuts[id_DIG_PIN_1].isDigOn();
    pidContainer.PID_PIN2 = mOuts[id_DIG_PIN_2].isDigOn(); */

    return 0;
}
