#pragma once
#include "app_state.h"
#include "app_sync.h"

// ============================================================================
// Thread-safe accessor wrapper für g_app.webSockData
// Nutzung von g_appMutex = kein doppeltes Lock (Deadlock) in bereits
// appLock()-geschützten Blöcken. Diese Funktionen sind FÜR Zugriffe
// AUSSERHALB bestehender Lock-Bereiche.
// ============================================================================

// --- STATES (Read) --------------------------------------------------------
static inline bool ws_getNetworkOK()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.networkOK;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getMqtt()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.mqtt;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getModbusOK()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.modbusOK;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getAmisReader()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.amisReader;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getTempSensorOK()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.tempSensorOK;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getFroniusAPI()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.froniusAPI;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getCardWriterOK()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.cardWriterOK;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getFlashOK()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.flashOK;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getInflux()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.influx;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getTimeServer()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.timeServer;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getBoilerHeating()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.boilerHeating;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline bool ws_getTempUnderflow()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.states.tempUnderflow;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

// --- STATES (Write) -------------------------------------------------------
static inline void ws_setNetworkOK(bool val)
{
    if (appLock(100))
    {
        g_app.webSockData.states.networkOK = val;
        appUnlock();
    }
}

static inline void ws_setFroniusAPI(bool val)
{
    if (appLock(100))
    {
        g_app.webSockData.states.froniusAPI = val;
        appUnlock();
    }
}

static inline void ws_setModbusOK(bool val)
{
    if (appLock(100))
    {
        g_app.webSockData.states.modbusOK = val;
        appUnlock();
    }
}

static inline void ws_setAmisReader(bool val)
{
    if (appLock(100))
    {
        g_app.webSockData.states.amisReader = val;
        appUnlock();
    }
}

static inline void ws_setTempSensorOK(bool val)
{
    if (appLock(100))
    {
        g_app.webSockData.states.tempSensorOK = val;
        appUnlock();
    }
}

static inline void ws_setMqtt(bool val)
{
    if (appLock(100))
    {
        g_app.webSockData.states.mqtt = val;
        appUnlock();
    }
}

static inline void ws_setTempUnderflow(bool val)
{
    if (appLock(100))
    {
        g_app.webSockData.states.tempUnderflow = val;
        appUnlock();
    }
}

// --- SETUP (Read) ---------------------------------------------------------
static inline unsigned int ws_getTempMaxAllowed()
{
    unsigned int result = 0;
    if (appLock(50))
    {
        result = g_app.webSockData.setupData.tempMaxAllowedInGrad;
        appUnlock();
    }
    return result;
}

static inline unsigned int ws_getTempMin()
{
    unsigned int result = 0;
    if (appLock(50))
    {
        result = g_app.webSockData.setupData.tempMinInGrad;
        appUnlock();
    }
    return result;
}

static inline int ws_getForceHeating()
{
    int result;
    if (appLock(50))
    {
        result = g_app.webSockData.setupData.forceHeating;
        appUnlock();
    }
    else
    {
        result = 0;
    }
    return result;
}

static inline bool ws_getSetupChanged()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.setupData.setupChanged;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

static inline Setup ws_getSetupCopy()
{
    Setup copy;
    if (appLock(100))
    {
        copy = g_app.webSockData.setupData;
        appUnlock();
    }
    return copy;
}

static inline char* ws_getSsid(char *buf, size_t len)
{
    if (appLock(50))
    {
        strncpy(buf, g_app.webSockData.setupData.ssid, len - 1);
        buf[len - 1] = '\0';
        appUnlock();
    }
    else
    {
        strncpy(buf, "", len - 1);
        buf[len - 1] = '\0';
    }
    return buf;
}

static inline char* ws_getPasswd(char *buf, size_t len)
{
    if (appLock(50))
    {
        strncpy(buf, g_app.webSockData.setupData.passwd, len - 1);
        buf[len - 1] = '\0';
        appUnlock();
    }
    else
    {
        strncpy(buf, "", len - 1);
        buf[len - 1] = '\0';
    }
    return buf;
}

static inline char* ws_getCurrentIP(char *buf, size_t len)
{
    if (appLock(50))
    {
        strncpy(buf, g_app.webSockData.setupData.currentIP, len - 1);
        buf[len - 1] = '\0';
        appUnlock();
    }
    else
    {
        strncpy(buf, "", len - 1);
        buf[len - 1] = '\0';
    }
    return buf;
}

// --- SETUP (Write) --------------------------------------------------------
static inline void ws_setSetupChanged(bool val)
{
    if (appLock(100))
    {
        g_app.webSockData.setupData.setupChanged = val;
        appUnlock();
    }
}

static inline void ws_setForceHeating(int val)
{
    if (appLock(100))
    {
        g_app.webSockData.setupData.forceHeating = val;
        appUnlock();
    }
}

static inline void ws_setSetup(const Setup &val)
{
    if (appLock(200))
    {
        g_app.webSockData.setupData = val;
        appUnlock();
    }
}

// --- TEMPERATURE (Read) ---------------------------------------------------
static inline int ws_getSensor1()
{
    int result;
    if (appLock(50))
    {
        result = g_app.webSockData.temperature.sensor1;
        appUnlock();
    }
    else
    {
        result = 0;
    }
    return result;
}

static inline int ws_getSensor2()
{
    int result;
    if (appLock(50))
    {
        result = g_app.webSockData.temperature.sensor2;
        appUnlock();
    }
    else
    {
        result = 0;
    }
    return result;
}

static inline bool ws_getTempAlarm()
{
    bool result;
    if (appLock(50))
    {
        result = g_app.webSockData.temperature.alarm;
        appUnlock();
    }
    else
    {
        result = false;
    }
    return result;
}

// --- TEMPERATURE (Write) --------------------------------------------------
static inline void ws_setTempAlarm(bool val)
{
    if (appLock(100))
    {
        g_app.webSockData.temperature.alarm = val;
        appUnlock();
    }
}

// --- LOW-LEVEL: Setup direct access for EEPROM / initial load (no lock) ---
static inline Setup& ws_getSetupRaw()
{
    return g_app.webSockData.setupData;
}

static inline WEBSOCK_DATA& ws_getDataRaw()
{
    return g_app.webSockData;
}
