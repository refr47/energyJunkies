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

    void setValue(double v)
    {
        if (mType == Digital)
        {
            if (v > 0.5)
            {
                mValue = 1;
                mActivationTime = millis();
            }
            else
            {
                mValue = 0;
            }
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
        if (millis() - mActivationTime > mMinOnTime)
        {
            return true;
        }
        return false;
    }

    int mPin;
    OutputType mType;
    int mActivationTime;
    int mMinOnTime;
    int mValue;
};

class PinManager
{
public:
    PinManager(int digOut1, int digOut2, int anOut);
    void config(Setup &setup);
    int task(Setup &setup, PID_CONTAINER &pidContainer);

    int getStateOfDigPin(short pin) // 0, 1
    {
        return mOuts[pin].isDigOn();
    }
    int getStateOfAnaPin()
    {
        return mOuts[2].mValue;
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
