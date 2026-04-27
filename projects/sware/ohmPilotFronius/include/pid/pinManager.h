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
    void apply(LogEntry &logEntry, int power);
    void reset();
    void testPins(int l1,int l2,int pwm );

private:

    // Hardware
    int pinL1, pinL2, pwmPin;

    // Power
    int onePhase;

    // State
    int currentPhases = 0;
    int currentPWM = 0;

    // Relay protection
    unsigned long lastSwitch = 0;
    const unsigned long MIN_SWITCH = 5000;
    //const unsigned int MAX_LEN_MEASURE = 12; // hysterese, glättung
    double lastSmoothedPower = 0;
    const int DEAD_BAND = DEAD_BAND_WATT; // Änderungen unter 50W werden ignoriert
    int lastTargetPower = 0;    // Speicher für den letzten Sollwert
    int rest = 0;

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
    /* const double MIN_TEMP = 45;
     const double MAX_TEMP = 70;
     const double LEG_TEMP = 60;
 */
    const int EPSILON_TEMP = 20;
   
 

    // Internal 
    int tempState(double t);
    int pvState(double p);
    std::vector<int> availablePower;
    int powerIndex;

    int heaterPower();
    int basePower(int effectivePower);
    ControlMode preCheck(WEBSOCK_DATA &webSockData, int temp, unsigned long nowMS);
    void fillLogEntry(WEBSOCK_DATA& webSockData, LogEntry& logEntry);
    int getMeanOfAvailAblePower();
#ifdef PHASEN2

    float P_surplus;      // aktueller PV-Überschuss
    float P_phase = 1000; // Leistung einer Heizphase in Watt

    bool relayState = false;
    float relay_on_threshold = 100;
    float relay_off_threshold = 80;
    int pwmValue = 0;
    unsigned long relayCandidateTimer = 0;
    unsigned long relayDelay = 10000; // 10 Sekunden
    void setRelaySafe(int pin, bool state, unsigned long &lastSwitch);
#endif
};
 