#pragma once
#include <Arduino.h>
#include "defines.h"
#include "TinyNN.h"

#include <vector>
#include <iostream>
#include <algorithm>

const double OUTPUT_MAX = 255.0;
#define DEAD_BAND_WATT 30.0
#define HYSTERESIS_WATT 12

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
    void update(WEBSOCK_DATA &webSockData, int tempMaxBonusC = 0);
    ~PinManager();
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
   
 

    // ── lokale Eingabedaten (thread-safe: aus appLock()-Kontext kopiert) ──
    int   m_sensor1;
    int   m_sensor2;
    bool  m_froniusAPI;
    bool  m_boilerHeating;
    int   m_tempMaxAllowed;
    int   m_tempMin;
    int   m_legionellenMaxTemp;
    unsigned long m_legionellenDelta;
    int   m_akkuPriori;
    float m_akkuLadung;
    float m_gridPower;       // Fronius / Modbus Inverter-Wert
    float m_meterPower;      // harmonisiert: Fronius p_load oder AMIS consumptionInWatt
    int   m_wattSetupForTest;

    // ── lokale Ausgabedaten (unter appLock() zurückgeschrieben) ──
    bool  m_out_boilerHeating;
    bool  m_out_wattBiasForTest;

    // Internal 
    int tempState(double t);
    int pvState(double p);
    std::vector<int> availablePower;
    int powerIndex;

    int  heaterPower();
    int  basePower(int effectivePower);
    void preCheck(int temp, unsigned long nowMS, ControlMode &mode, bool &doML);
    int  getMeanOfAvailAblePower();

    // Hilfs-Methoden für thread-safe lock/Unlock
    void pidLockRead(WEBSOCK_DATA& webSockData);
    void pidLockWrite(WEBSOCK_DATA& webSockData);

    // ── update() helpers ──
    int   resolveInputTemp() const;
    void  handleModeOff(LogEntry &logEntry);
    void  handleForceHeating(LogEntry &logEntry, int &measuredPower);
    void  handleModeManual();
    int   handleModeAuto(LogEntry &logEntry, int &measuredPower, bool &doML);
    float computeMLReward(int measuredPower, int targetPower, int temp);
    void  logStackWarning();

};
 