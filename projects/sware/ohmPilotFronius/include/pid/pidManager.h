#pragma once

#include <Arduino.h>
// #include <PID_v1.h>
#include "defines.h"
#include <vector>
#include <iostream>
#include <algorithm>

#define MAX_LEN_MEASURE 12 // length of array for storing measure - @see: PID_CONTROLLER_INTERVALL in main ( currently: 1000 ms)

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
        // DBGf("Pin %d, activTime: %d", mPin, mActivationTime);
        if (millis() - mActivationTime > mMinOnTime)
        {
            // DBGf("Pin free");
            return true;
        }
        // DBGf("Pin occupied by time");
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
    void allOn(); 
    void switchOnL1();
    void switchOnL2();
    void switchOnL3();

    bool task(WEBSOCK_DATA &webSockData); // > 0: bezug vom Netz, <0 eigene Produktion

    int getStateOfDigPin(short pin) // 0, 1
    {
        return mOuts[pin].isDigOn();
    }
    int getStateOfAnaPin()
    {
        return mOuts[2].mValue;
    }
    double getWattBoundInRelays();
    double reduceRelayStorage();
    double addRelayStorage();
    void adjustPWM();

private:
    bool prologTemperature(WEBSOCK_DATA &webSockData);
    bool prologExternalBoilerSwitchHandling(WEBSOCK_DATA &webSockData);

private:
    Pins mOuts[3];
    double mAnalogOut;
    double onePhase;
    double availableWatt;
    double gridWatt;
    double storage;
    unsigned int boilerSwitchExternalOn; // bimetal boiler switched off
    unsigned long testForBoilerSwitch;
    // double mCurrentPower;

    // PID mPid;

    std::vector<double> availablePower;
    int powerIndex;
    double getMeanOfAvailAblePower();

    unsigned long mDelayDigOutOn;
    unsigned long mDelayDigOutOff;
};
