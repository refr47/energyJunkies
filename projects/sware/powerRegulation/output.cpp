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

// max power which is consumed on an output
static const int maxPower = 2000;
// reduce power setpoint by this value
static const int powerDiff = 50;



static const int idxDig1 = 0;
static const int idxDig2 = 1;
static const int idxAn = 2;

OutputManager::OutputManager(int digOut1, int digOut2, int anOut)
  : mPid(&mCurrentPower, &mAnalogOut, &mPidSetPoint, KP, KI, KD, REVERSE) {
  mOuts[idxDig1].init(digOut1, maxPower, Digital);
  mOuts[idxDig2].init(digOut2, maxPower, Digital);
  mOuts[idxAn].init(anOut, maxPower, Analog);
  
  mPidSetPoint = powerDiff;
  mPid.SetMode(AUTOMATIC);
  mPid.SetOutputLimits(0, 255);
  mPid.SetSampleTime(50);
  
}

int OutputManager::task(int power) {
  // if (power < 0) {
  //   power *= -1;
  // }

  mCurrentPower = power;
  mPid.Compute();

  dbgln("=== task ===");
  dbg("power:");
  dbg(power);
  dbg("    mCurrPower:");
  dbg(mCurrentPower);
  dbg("    anaOut:");
  dbg(mAnalogOut);
  dbg("    setpoint:");
  dbg(mPidSetPoint);
  dbgln("");
  dbgln("============");

  unsigned long t = millis();
  if (mCurrentPower > mPidSetPoint && mAnalogOut > OUTPUT_MAX - 1) {
    // Analog regulator is on full load but power value is still to high
    // TODO: wait some time and start digital output
  }

  mOuts[idxAn].setValue(mAnalogOut);

  return 0;
}
