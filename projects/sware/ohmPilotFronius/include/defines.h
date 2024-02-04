#pragma once
// influx, curTime:
#define NtpServer1 "pool.ntp.org"
#define NtpServer2 "time.nist.gov"
#define EUROPE_VIENNA_TZ "CET-1CEST,M3.5.0,M10.5.0/3" // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

#define LEN_WLAN 30
#define MODBUS_PORT "502"
#define AMIS_KEY_LEN 35
#define JSON_OBJECT_SETUP_LEN 1024 /// www.cpp,

#define INVERTER_DATA webSockData.mbContainer.inverterSumValues.data
#define METER_DATA webSockData.mbContainer.meterValues.data
#define AKKU_STATE webSockData.mbContainer.akkuState.data
#define AKKU_STRG webSockData.mbContainer.akkuStr.data
#define FRONIUS webSockData.fronius_SOLAR_POWERFLOW


#include "debugConsole.h"

#ifdef FRONIUS_IV
#include "modbusRegister.h"
#elif HUAWEI_IV
#include "huaweiDefines.h"
#endif

typedef struct
{
    char ssid[LEN_WLAN];
    char passwd[LEN_WLAN];

    unsigned int heizstab_leistung_in_watt;
    unsigned int phasen_leistung_in_watt; // heizstab_leistung_in_watt  pre calculation @see: eprom_getSetup
    unsigned int tempMaxAllowedInGrad;
    unsigned int tempMinInGrad;
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
    unsigned int pid_powerWhichNeedNotConsumed; // Wieviel müss übrig bleiben
    bool pidChanged;
    double additionalLoad;
    int exportWatt;
    unsigned int ipAmisReaderHost;
    char amisKey[AMIS_KEY_LEN];
    String amisReaderHost;
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

typedef struct _STATES
{
    bool cardWriterOK;
    bool networkOK;
    bool modbusOK;
    bool flashOK;
    bool tempSensorOK;
    bool froniusAPI;
} STATES;

typedef struct _FRONIUS_SOLAR_POWERFLOW
{
    double p_akku;
    double p_grid;
    double p_load;
    double p_pv;
    unsigned int rel_Autonomy;
    unsigned int rel_SelfConsumption;
} FRONIUS_SOLAR_POWERFLOW;
typedef struct _WEBSOCK
{
    MB_CONTAINER mbContainer;
    Setup setup;
    TEMPERATURE temperature;
    PID_CONTAINER pidContainer;
    STATES states;
    Setup setupData;
#ifdef FRONIUS_API
    FRONIUS_SOLAR_POWERFLOW fronius_SOLAR_POWERFLOW;
#endif

} WEBSOCK_DATA;

typedef WEBSOCK_DATA &(*CALLBACK_GET_DATA)();
