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

    // Legionella
    unsigned long lastLegionella = 0;
    bool legionella = false;



    // ── lokale Eingabedaten (thread-safe: aus appLock()-Kontext kopiert) ──
    int   m_sensor1;
    int   m_sensor2;
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

    // ── update() steps ──
    void  stepPrepare(WEBSOCK_DATA &webSockData, int tempMaxBonusC,
                      float pvRatio, float cloudAvg, float tempOutside);
    bool  stepForceOff(int temp, unsigned long now, WEBSOCK_DATA &webSockData);
    bool  stepForceHeat(int temp, int effectiveMax, unsigned long now,
                        WEBSOCK_DATA &webSockData);
    void  stepControlMode(int temp, int effectiveMax, unsigned long now,
                          WEBSOCK_DATA &webSockData);

    // ── per-mode executors ──
    void  execModeOff(LogEntry &logEntry);
    void  execModeLegionella(int temp, unsigned long now, WEBSOCK_DATA &webSockData);
    void  execModeMinTemp(int temp, unsigned long now, WEBSOCK_DATA &webSockData);
    void  execModeAuto(int temp, unsigned long now, bool doML,
                       int effectiveMax, WEBSOCK_DATA &webSockData);
};
 