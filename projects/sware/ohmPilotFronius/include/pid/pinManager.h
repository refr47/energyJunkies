#pragma once
#include <Arduino.h>
#include "defines.h"
#ifdef TINYNN_ENABLE
#include "TinyNN.h"
#endif

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
    PinManager();
    void config(WEBSOCK_DATA &data, int l1, int l2, int pwm);
    void update(WEBSOCK_DATA &webSockData, int tempMaxBonusC,
                float pvRatio, float cloudAvg, float tempOutside);
    ~PinManager();
    // helper
    void allOn();
    void allOff();
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
    unsigned long lastSwitchL1 = 0;
    unsigned long lastSwitchL2 = 0;
    unsigned long lastSwitchPwm = 0;
    const unsigned long MIN_SWITCH_L1 = 10000; // 10s Hysterese Relay 1
    const unsigned long MIN_SWITCH_L2 = 10000; // 10s Hysterese Relay 2
    const unsigned long MIN_SWITCH_PWM = 2000; // 2s PWM
    //const unsigned int MAX_LEN_MEASURE = 12; // hysterese, glättung
    double lastSmoothedPower = 0;
    const int DEAD_BAND = DEAD_BAND_WATT; // Änderungen unter 50W werden ignoriert
    int lastTargetPower = 0;    // Speicher für den letzten Sollwert
    int rest = 0;

    // EMA smoothing for PV surplus (α=0.2 → effektives Fenster ≈ 5 Werte)
    double emaSurplus = 0;
    bool   emaInitialized = false;

#ifdef TINYNN_ENABLE
    TinyNN *tinyNN;
#endif

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
    float m_gridPower;       // Grid-Saldo (W), neg = Einspeisung

    float m_pV;              // PV-Ertrag (W)
    float m_soc;             // Batterie SoC (%)
    float m_battPower;       // Batterieleistung (W), neg = laden
    int   m_wattSetupForTest;
    int   m_forceHeating;   // 0=auto, 1=L1, 2=L1+L2, 3=ALLE
    int   m_forceHeatingStop; // 0=normal, 1=HEATER KILL (alles OFF)

    // ── lokale Ausgabedaten (unter appLock() zurückgeschrieben) ──
    bool  m_out_boilerHeating;
    bool  m_out_wattBiasForTest;

    // ── Prediction-Member (für pidLockWrite → WebSocket → Client) ──
    int   m_weatherBonus;
    float m_weatherPvRatio;
    float m_weatherCloudAvg;
    float m_weatherOutTemp;
#ifdef TINYNN_ENABLE
    float m_tinyNN_preheat;
    float m_tinyNN_buffer;
#endif
 
    // Internal helpers für Log- & State-Commit
    static void fillEntry(LogEntry &le, unsigned long now,
                         uint8_t state, int power, uint8_t pwm, int temp);
    void  commit(LogEntry &le, WEBSOCK_DATA &webSockData);

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
    int   handleModeAuto(LogEntry &logEntry, int &measuredPower, bool &doML);
    float computeMLReward(int measuredPower, int targetPower, int temp);
    void  logStackWarning();

};
 