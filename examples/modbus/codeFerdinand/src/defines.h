#pragma once
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <modbus/modbus.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <float.h>
#include <string.h>
#define LEN_WLAN 30
#define MODBUS_PORT "502"
#define JSON_OBJECT_SETUP_LEN 1024 /// www.cpp,
typedef unsigned char byte;
typedef unsigned char u8_t;
typedef unsigned int uint32_t;

#include "debugConsole.h"
#include "modbusRegister.h"

typedef struct
{
    char ssid[LEN_WLAN];
    char passwd[LEN_WLAN];

    unsigned int heizstab_leistung_in_watt;
    unsigned int phasen_leistung_in_watt; // heizstab_leistung_in_watt  pre calculation @see: eprom_getSetup
    unsigned int tempMaxAllowedInGrad;
    unsigned int tempMinInGrad;
    unsigned int ipInverter;
    char *ipInverterAsString;
    bool externerSpeicher;
    char externerSpeicherPriori;
    float pid_p;
    float pid_i;
    float pid_d;
    unsigned int pid_min_time_without_contoller_inMS;
    unsigned int pid_min_time_before_switch_off_channel_inMS;
    unsigned int pid_min_time_for_dig_output_inMS;
    unsigned int pid_powerWhichNeedNotConsumed; // Wieviel müss übrig bleiben
    bool pidChanged;
    char testPid;
    int exportWatt;
} Setup;

typedef struct
{
    bool alarm;
    float sensor1;
    float sensor2;
} TEMPERATURE;

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

typedef struct _STATES
{
    bool cardWriterOK;
    bool networkOK;
    bool modbusOK;
    bool flashOK;
    bool tempSensorOK;
} STATES;

// typedef WEBSOCK_DATA &(*CALLBACK_GET_DATA)();
