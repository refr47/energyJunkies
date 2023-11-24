#pragma once

#define LEN_WLAN 30
#define MODBUS_PORT "502"
#define JSON_OBJECT_SETUP_LEN 1024 /// www.cpp,
#include "debugConsole.h"
#include "modbusRegister.h"

typedef struct
{
    char ssid[LEN_WLAN];
    char passwd[LEN_WLAN];

    unsigned int regelbereichHysterese;
    unsigned int ausschaltTempInGradCel;
    unsigned int ipInverter;
    String ipInverterAsString;
    bool externerSpeicher;
    char externerSpeicherPriori;
    float pid_p;
    float pid_i;
    float pid_d;
    unsigned int pid_min_time_without_contoller_inMS;
    unsigned int pid_min_time_before_switch_off_channel_inMS;
    unsigned int pid_min_time_for_dig_output_inMS;
    unsigned int pid_targetPowerInWatt; // Wieviel müss übrig bleiben
    bool pidChanged;
    char testPid;
    unsigned int exportWatt;
} Setup;

typedef struct
{
    bool alarm;
    float sensor1;
    float sensor2;
} TEMPERATURE;
typedef struct mbContainer
{
    INVERTER_SUM_VALUE_t inverterSumValues;
    METER_VALUE_t meterValues;
    AKKU_STATE_VALUE_t akkuState;
    AKKU_STRG_VALUE_t akkuStr;

} MB_CONTAINER;

typedef struct pidContaienr
{

    double mCurrentPower;
    int mAnalogOut;
    int powerNotUseable; // power, die nicht verbraucht werden darf
    int PID_PIN1;
    int PID_PIN2;
} PID_CONTAINER;

typedef struct _LIFE_DATA
{
    long tempLimitReached; // timestamp
    long heatingLastTime;

} LIFE_DATA;

typedef struct _WEBSOCK
{
    MB_CONTAINER mbContainer;
    Setup setup;
    TEMPERATURE temperature;
    PID_CONTAINER pidContainer;

} WEBSOCK_DATA;

typedef WEBSOCK_DATA &(*CALLBACK_GET_DATA)();
typedef struct _STATES
{
    bool cardWriterOK;
    bool networkOK;
    bool modbusOK;
    bool flashOK;
    bool tempSensorOK;
} STATES;
