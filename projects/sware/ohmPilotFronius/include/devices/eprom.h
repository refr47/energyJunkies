#pragma once

#include "defines.h"


#define _SSID "ssid"
#define _PASSWORD "pw"
#define _HEIZSTAB_LEISTUNG_IN_WATT "hy"
#define _INVERTER_IP "ip"
#define _TEMP_MAX_IN_GRAD "ausT"
#define _TEMP_MIN_IN_GRAD "einT"
#define _EXTERNER_SPEICHER "eS"
#define _EXTERNER_SPEICHER_PRIORI "esP"
/* #define _PID_P "pid_p"
#define _PID_I "pid_i"
#define _PID_D "pid_d" */
#define _LEGIONELLEN_SCHWELLWERT_DELTA_TIME "legiTime"
#define _LEGIONELLEN_SCHWELLWERT_DELTA_TEMP "legiTemp"
////#define _PID_DIG_OUT_ON_DELAY_MS "doD" // power has to be above set point before controller is invoked
/* #define _PID_DIG_OUT_OFF_DELAY_MS "ddms" // minimal time before pid controller turns off channel/pin
#define _PID_MIN_ON_TIME_MS "dmin"       // minimal on time for digital output (in ms) */
//#define _PID_TARGET_POWER "tpow" // target power - max available power

/* #define _PID_TEST "pT" */
#define _EN_FORCE_HEATING "eX"
#define _EPSILON_PIN_MANAGER "enL"

#define _AMIS_READER_HOST "arH"
#define AMIS_READER_HOST_DEFAULT "AmisReader-Host"
#define _AMIS_READER_KEY "arK"

#define _MQTT_HOST "mqH"
#define _MQTT_USER "mqU"
#define _MQTT_PASSWD "mqP"

#define _INFLUX_HOST "infH"
#define _INFLUX_TOKEN "infT"
#define _INFLUX_ORG "infO"
#define _INFLUX_BUCKET "infB"

#define EMPTY_STRING "\0"
void eprom_storeSetup(Setup &setup);
void eprom_getSetup(Setup &setup);
void eprom_isInit();
// String& eprom_getInverter(Setup &setup, String &inverter);

void eprom_test_write_Eprom(const char *, const char *);
void eprom_test_read_Eprom();
void eprom_show(Setup &setup);

void eprom_store_shelly(ALL_SHELLY_DEVICES *allDevices, unsigned upperLimit);

void eprom_clearLifeData();
void eprom_getLifeData(LIFE_DATA &data);

bool eprom_stammDataUpdate();
void eprom_stammDataUpdateReset();
void printEprom(Setup &setup);

#define _LIFE_DATA "lifeD"
#define _TEMP_LIMIT_REACHED "t"
#define _HEATING_SWITCHED_ON_LAST_TIME "h"