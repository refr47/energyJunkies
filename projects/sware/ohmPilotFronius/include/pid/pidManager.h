#pragma once

#include <Arduino.h>
#include <PID_v1.h>
#include "defines.h"

enum OutputType
{
    Analog,
    Digital
};

class Pins
{
    friend class PinManager;

private:
    void init(int pin, int onTime, OutputType type)
    {
        mPin = pin;
        mMinOnTime = onTime;
        mType = type;
        pinMode(mPin, OUTPUT);
    }

    void setValue(int v)
    {
        if (mType == Digital)
        {
            mActivationTime = millis();
            mValue = v;
            digitalWrite(mPin, mValue);
        }
        else
        {
            mValue = v;
            analogWrite(mPin, mValue);
        }
    }

    bool isDigOn(void)
    {
        if (mType == Digital && mValue > 0.5)
        {
            return true;
        }
        return false;
    }

    bool hasActivationTimeElapsed(void)
    {
        DBGf("Pin %d, activTime: %d", mPin, mActivationTime);
        if (millis() - mActivationTime > mMinOnTime)
        {
            DBGf("Pin free");
            return true;
        }
        DBGf("Pin occupied");
        return false;
    }

    int mPin;
    OutputType mType;
    unsigned long mActivationTime;
    int mMinOnTime;
    int mValue;
};

class PinManager
{
public:
    PinManager();
    void config(Setup &setup, int digOut1, int digOut2, int anOut);
    void reset(); //

    bool task(Setup &setup, double *currentAvailablePower); // > 0: bezug vom Netz, <0 eigene Produktion

    int getStateOfDigPin(short pin) // 0, 1
    {
        return mOuts[pin].isDigOn();
    }
    int getStateOfAnaPin()
    {
        return mOuts[2].mValue;
    }
    double getCurrentPower()
    {
        return mCurrentPower;
    }
    double getReservedPower()
    {
        return mPidSetPoint;
    }

private:
    Pins mOuts[3];
    double mAnalogOut;
    double mPidSetPoint;
    double mCurrentPower;

    PID mPid;

    unsigned long mDelayDigOutOn;
    unsigned long mDelayDigOutOff;
};
