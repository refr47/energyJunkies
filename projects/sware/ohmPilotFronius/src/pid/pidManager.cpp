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
static int DELAY_DIG_OUT_ON = 5000;
// delay turning of next digital output for this time (in ms)
static int DELAY_DIG_OUT_OFF = 2000;

// minimal on time for digital output (in ms)
static int MIN_ON_TIME = 10000;
// target power - max available power
static int TARGET_POWER = 5950;

static const int idxDig1 = 0;
static const int idxDig2 = 1;
static const int idxAn = 2;

PinManager::PinManager(int digOut1, int digOut2, int anOut)
    : mPid(&mCurrentPower, &mAnalogOut, &mPidSetPoint, KP, KI, KD, REVERSE)
{
    mOuts[idxDig1].init(digOut1, MIN_ON_TIME, Digital);
    mOuts[idxDig2].init(digOut2, MIN_ON_TIME, Digital);
    mOuts[idxAn].init(anOut, MIN_ON_TIME, Analog);

    mDelayDigOutOn = millis();
    mDelayDigOutOff = millis();

    mPidSetPoint = TARGET_POWER;
    mPid.SetMode(AUTOMATIC);
    mPid.SetOutputLimits(OUTPUT_MIN, OUTPUT_MAX);
    mPid.SetSampleTime(50);
}
void PinManager::config(Setup &setup)
{
    DELAY_DIG_OUT_ON = setup.pid_min_time_without_contoller_inMS;
    DELAY_DIG_OUT_OFF = setup.pid_min_time_before_switch_off_channel_inMS;
    MIN_ON_TIME = setup.pid_min_time_for_dig_output_inMS;
    TARGET_POWER = setup.pid_targetPowerInWatt;
}
int PinManager::task(int power)
{
    mCurrentPower = power;
    mPid.Compute();
    mOuts[idxAn].setValue(mAnalogOut);

    // turn on digital output if power suffices
    if (mCurrentPower > mPidSetPoint && mAnalogOut > OUTPUT_MAX - 1)
    {
        if (millis() - mDelayDigOutOn > DELAY_DIG_OUT_ON)
        {
            for (int i = idxDig1; i <= idxDig2; i++)
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
    if (mCurrentPower < mPidSetPoint && mAnalogOut < OUTPUT_MIN + 1 && millis() - mDelayDigOutOff > DELAY_DIG_OUT_OFF)
    {
        for (int i = idxDig2; i >= idxDig1; i--)
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
    /* dbg("    mCurrPower:");
    dbg(mCurrentPower);
    dbg("    anaOut:");
    dbg(mAnalogOut);
    dbg("    setpoint:");
    dbg(mPidSetPoint);
    dbg("    dig1:");
    dbg(mOuts[idxDig1].isDigOn());
    dbg("    dig2:");
    dbg(mOuts[idxDig2].isDigOn());
    dbgln("");
 */
    return 0;
}
