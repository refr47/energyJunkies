#pragma once
#include <Arduino.h>
#include "defines.h"
#include "TinyNN.h"

#include <vector>
#include <iostream>
#include <algorithm>

const double OUTPUT_MAX = 255.0;
#define DEAD_BAND_WATT 30.0

enum ControlMode
{
    MODE_OFF,        // Alles aus (Max Temp erreicht)
    MODE_LEGIONELLA, // Vollgas (Legionellen-Programm)
    MODE_MIN_TEMP,   // Vollgas (Frostschutz / Min-Temp)
    MODE_MANUAL,     // Manuelle Steuerung
    MODE_AUTO        // RL / PID Controller übernimmt
};

class PinManager
{
public:
    void config(WEBSOCK_DATA &data, int l1, int l2, int pwm);
    void update(WEBSOCK_DATA &webSockData/*, double temp, int hour*/);
    // helper
    void allOn();
    int getStateOfDigPin(short pin);
    int getStateOfAnaPin();
    void apply(LogEntry &logEntry, double power);
    void reset();

private:

    // Hardware
    int pinL1, pinL2, pwmPin;

    // Power
    double onePhase;

    // State
    int currentPhases = 0;
    double currentPWM = 0;

    // Relay protection
    unsigned long lastSwitch = 0;
    const unsigned long MIN_SWITCH = 5000;
    const unsigned int MAX_LEN_MEASURE = 12; // hysterese, glättung
    double lastSmoothedPower = 0;
    const double DEAD_BAND = DEAD_BAND_WATT; // Änderungen unter 50W werden ignoriert
    double lastTargetPower = 0;    // Speicher für den letzten Sollwert
    double rest = 0;

    TinyNN *tinyNN;

    // RL
   /*  static const int S_T = 5;
    static const int S_P = 5;
    static const int A = 5;
    double Q[S_T][S_P][A]; */

    double epsilon = 0.05;
    double alpha = 0.1;

    // Legionella
    unsigned long lastLegionella = 0;
    bool legionella = false;

    // Config
    const double MIN_TEMP = 45;
    const double MAX_TEMP = 70;
    const double LEG_TEMP = 60;

    const double EPSILON_TEMP = 20.0;
   


    // Internal 
    int tempState(double t);
    int pvState(double p);
    /* int chooseAction(int t, int pv);
    double actionFactor(int a); */
    std::vector<double> availablePower;
    int powerIndex;

    double heaterPower();
    double basePower(double effectivePower);
    ControlMode preCheck(WEBSOCK_DATA &webSockData, double temp, unsigned long nowMS, LogEntry &logEntry);

    double getMeanOfAvailAblePower();
};
