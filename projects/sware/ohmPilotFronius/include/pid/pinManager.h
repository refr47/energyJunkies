#pragma once
#include <Arduino.h>
#include "defines.h"
#ifdef TINYNN_ENABLE
#include "TinyNN.h"
#endif

#include <algorithm>

#include "phaseController.h"
#include "batteryMonitor.h"
#include "mlPredictor.h"
#include "thermalController.h"
#include "logCommitter.h"


#define DEAD_BAND_WATT 30.0
#define HYSTERESIS_WATT 12



class PinManager
{
public:
    PinManager();
    void config(WEBSOCK_DATA &data, int l1, int l2, int pwm);
    void update(WEBSOCK_DATA &webSockData, int tempMaxBonusC,
                float pvRatio, float cloudAvg, float tempOutside);

    // ── Hardware delegation (wrapped PhaseController calls) ────────────────
    void allOn() { phaseCtrl.allOn(); }
    void allOff() { phaseCtrl.allOff(); }
    void testPins(int, int, int);
    ~PinManager();
    // helper
    int getStateOfDigPin(short pin) ;
    int getStateOfAnaPin();
    void reset();



private:

        // ── Hardware controller (relay + PWM) ──
    PhaseController phaseCtrl;

    // ── Battery & Grid Monitor ──
    BatteryMonitor batteryMon;

    // ── ML Prediction & Training ──
    MlPredictor mlPredictor;

    // ── State ────────────────────────────────────────────────────────────
    double lastSmoothedPower = 0;


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
    bool  m_boilerHeating;
    int   m_tempMaxAllowed;
    int   m_tempMin;
    int   m_legionellenMaxTemp;
    unsigned long m_legionellenDelta;

    int   m_forceHeating;   // 0=auto, 1=L1, 2=L1+L2, 3=ALLE
    int   m_forceHeatingStop; // 0=normal, 1=HEATER KILL (alles OFF)

    // ── lokale Ausgabedaten (unter appLock() zurückgeschrieben) ──
    bool  m_out_boilerHeating;
    bool  m_out_wattBiasForTest = false;

    // ── Prediction-Member (für pidLockWrite → WebSocket → Client) ──
    int   m_weatherBonus;
    float m_weatherPvRatio;
    float m_weatherCloudAvg;
    float m_weatherOutTemp;
#ifdef TINYNN_ENABLE
    float m_tinyNN_preheat;
    float m_tinyNN_buffer;
#endif
 


    int  resolveInputTemp() const;
    void preCheck(int temp, unsigned long nowMS, ControlMode &mode, bool &doML);

    // Hilfs-Methoden für thread-safe lock/Unlock
    void pidLockRead(WEBSOCK_DATA& webSockData);
    void pidLockWrite(WEBSOCK_DATA& webSockData);

    // ── update() helpers ──
    void  handleModeOff(LogEntry &logEntry);
    int   handleModeAuto(LogEntry &logEntry, int &measuredPower, bool &doML);
};
 