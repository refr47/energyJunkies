#pragma once
#include <Arduino.h>
#include <arpa/inet.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

// influx, curTime:
#define NtpServer1 "pool.ntp.org"
#define NtpServer2 "time.nist.gov"
#define EUROPE_VIENNA_TZ "CET-1CEST,M3.5.0,M10.5.0/3" // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

#define LEN_WLAN 30
#define MODBUS_PORT "502"

#define AMIS_KEY_LEN 35
#define AMIS_HOST_LEN 50
#define MQTT_HOST_LEN 50
#define MQTT_USER_LEN 25
#define MQTT_PASS_LEN 25
#define INFLUX_HOST_LEN 50
#define INFLUX_TOKEN_LEN 95
#define INFLUX_ORG_LEN 30
#define INFLUX_BUCKET_LEN 30

#define JSON_OBJECT_SETUP_LEN 1024 /// www.cpp,

#define INVERTER_DATA webSockData.mbContainer.inverterSumValues.data
#define METER_DATA webSockData.mbContainer.meterValues.data
#define AKKU_STATE webSockData.mbContainer.akkuState.data
#define AKKU_STRG webSockData.mbContainer.akkuStr.data
#define FRONIUS webSockData.fronius_SOLAR_POWERFLOW

/* ****************** SOCKETS *****************/
// length of ip address string
#define IP_LENGTH 64
// host name length
#define HOSTNAME_LENGTH 64
// length of the resource identifier
#define RESOURCE_LENGTH 64
// HTTP rest API request length
#define REQUEST_LENGTH 64
// length of json object key
#define KEY_LENGTH 32
#define TARGET_NAME_LEN 24

/*    sock*/
#define REST_TARGET_COUNT 2
#define AMIS_READER_INDEX 0
#define FRONIUS_SOLAR_API_INDEX 1
#define AMIS_VALUE_COUNT 3
#define FRONIUS_VALUE_COUNT 6

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
    char inverterAsString[INET_ADDRSTRLEN];

    bool externerSpeicher;
    char externerSpeicherPriori;
    unsigned int pid_min_time_without_contoller_inMS;
    /*   float pid_p;
      float pid_i;
      float pid_d;

      unsigned int pid_min_time_before_switch_off_channel_inMS;
      unsigned int pid_min_time_for_dig_output_inMS;
      */
    unsigned int pid_powerWhichNeedNotConsumed; // Wieviel müss übrig bleiben
    // bool pidChanged;
    unsigned int ipAmisReaderHost;
    char amisKey[AMIS_KEY_LEN];

    char amisReaderHost[INET_ADDRSTRLEN];
    char mqttHost[MQTT_HOST_LEN];
    char mqttUser[MQTT_USER_LEN];
    char mqttPass[MQTT_PASS_LEN];

    char influxHost[INFLUX_HOST_LEN];
    char influxToken[INFLUX_TOKEN_LEN];
    char influxOrg[INFLUX_ORG_LEN];
    char influxBucket[INFLUX_BUCKET_LEN];

    double additionalLoad;
    int exportWatt;

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
    bool amisReader;
    bool influx;
    bool mqtt;
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

typedef struct _AMIS_READER
{
    long absolutImportInkWh;
    long absolutExportInkWh;
    long saldo;

} AMIS_READER;
typedef struct _WEBSOCK
{
    MB_CONTAINER mbContainer;
    // Setup setup;
    TEMPERATURE temperature;
    PID_CONTAINER pidContainer;
    STATES states;
    Setup setupData;

    FRONIUS_SOLAR_POWERFLOW fronius_SOLAR_POWERFLOW;

    AMIS_READER amisReader;

} WEBSOCK_DATA;

typedef WEBSOCK_DATA &(*CALLBACK_GET_DATA)();

typedef struct
{
    // (start) index of source register
    char key[KEY_LENGTH];
    // index of destination field element
    int valueIndex;
} KEY_VALUE_MAP_t;

typedef struct
{
    // rest API target name
    char text[TARGET_NAME_LEN];
    // host name of the rest API target
    char hostname[HOSTNAME_LENGTH];
    // server details
    struct sockaddr_in serverAddr;
    // rest API port
    uint16_t port;
    // rest API resource identifier
    char resource[RESOURCE_LENGTH];
    // complete rest API HTTP request
    char request[REQUEST_LENGTH];
    // file descriptor of connection socket
    int socketFd;
    // number of values to build from json object
    int valueCount;
    // json object key to values mapping array
    KEY_VALUE_MAP_t *mapping;
} HTTP_REST_TARGET_t;

typedef void (*GET_JSON_DATA)(HTTP_REST_TARGET_t *, char *, WEBSOCK_DATA &);