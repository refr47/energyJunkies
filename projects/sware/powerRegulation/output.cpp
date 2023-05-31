#include "PID_v1.h"
#include "esp32-hal.h"


#include "output.h"

#define DEBUG_OUTPUT
#ifdef DEBUG_OUTPUT
  #define dbgln(a) (Serial.println(a))
  #define dbg(a) (Serial.print(a))
#else
  #define dbgln(a)
  #define dbg(a)
#endif

//pid settings and gains
#define OUTPUT_MIN 0
#define OUTPUT_MAX 255
#define KP 1
#define KI 0.5
#define KD 0

// power has to be above set point for this time, before next digital output
// is activated (in ms)
#define DELAY_DIG_OUT 10000

// minimal on time for digital output (in ms)
static const int minOnTime = 10000;
// reduce power setpoint by this value
static const int powerDiff = 50;



static const int idxDig1 = 0;
static const int idxDig2 = 1;
static const int idxAn = 2;

OutputManager::OutputManager(int digOut1, int digOut2, int anOut)
  : mPid(&mCurrentPower, &mAnalogOut, &mPidSetPoint, KP, KI, KD, REVERSE) {
  mOuts[idxDig1].init(digOut1, minOnTime, Digital);
  mOuts[idxDig2].init(digOut2, minOnTime, Digital);
  mOuts[idxAn].init(anOut, minOnTime, Analog);

  mDelayDigOut = millis();

  mPidSetPoint = powerDiff;
  mPid.SetMode(AUTOMATIC);
  mPid.SetOutputLimits(OUTPUT_MIN, OUTPUT_MAX);
  mPid.SetSampleTime(50);

}

int OutputManager::task(int power) {
  // if (power < 0) {
  //   power *= -1;
  // }
  dbgln("=== task ===");


  mCurrentPower = power;
  mPid.Compute();

  unsigned long t = millis();
  // turn on digital output if power suffices
  if (mCurrentPower > mPidSetPoint && mAnalogOut > OUTPUT_MAX - 1) {
    if (millis() - mDelayDigOut > DELAY_DIG_OUT) {
      for (int i = idxDig1; i <= idxDig2; i++) {
        if (!mOuts[i].isDigOn()) {
          mOuts[i].setValue(1);
          mDelayDigOut = millis(); //delay next output
          break; // do not turn on the next output 
        }
      }

    }
  } else {
    mDelayDigOut = millis();
  }
  // turn off digital output if power is low
  if (mCurrentPower < mPidSetPoint && mAnalogOut < OUTPUT_MIN + 1) {
    for (int i = idxDig2; i >= idxDig1; i--) {
        if (mOuts[i].isDigOn() && mOuts[i].hasActivationTimeElapsed()) {
          mOuts[i].setValue(0);
          break; // do not turn off the next output 
        }
      }
  }

  mOuts[idxAn].setValue(mAnalogOut);

  // dbg("power:");
  // dbg(power);
  dbg("    mCurrPower:");
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
  dbgln("============");


  return 0;
}
