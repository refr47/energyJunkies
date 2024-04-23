#pragma once
/* Reihenfolge ist wichtig - ansonst fehler from rtos*/
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include <AsyncJson.h>
#include "debugConsole.h"
#include "defines.h"

#define WLAN_ESSID "WLAN_ESSID"
#define WLAN_PASSWD "WLAN_Password"
#define IP_INVERTER "IP_Inverter"
#define HEIZSTABLEISTUNG "Heizstableistung"
#define EINSPEISUNG_MUSS "Mindest_Einspeisung"
/* #define MINDEST_LAUFZEIT_DIGITALER_OUT "Mindeslaufzeit_Digital"
#define MINDEST_LAUFZEIT_PORT_ON "Mindeslaufzeit_Phase" */
#define MINDEST_LAUFZEIT_REGLER_KONSTANT "Mindeslaufzeit_Regler"
#define EXTERNER_SPEICHER "Speicher"
#define EXTERNER_SPEICHER_PRIORI "Speicher_Prioritaet"
#define TEMP_AUSSCHALTEN "Ausschalt_Temperatur"
#define TEMP_EINSCHALT "Einschalt_Temperatur"
/* #define PID_P "Ausgangsregler (P-Anteil)"
#define PID_I "Ausgangsregler (I-Anteil)"
#define PID_D "Ausgangsregler (D-Anteil)" */

#define WWW_MQTT_HOST "MQTT-Server"
#define WWW_MQTT_USER "MQTT-User"
#define WWW_MQTT_PASWWD "MQTT-Password"
#define WWW_INFLUX_HOST "Influx-Server"
#define WWW_INFLUX_TOKEN "Influx-Token"
#define WWW_INFLUX_ORG "Influx-Org"
#define WWW_INFLUX_BUCKET "Influx-Bucket"

#define AMIS_READER_HOST "Amis Reader Host (TCP/IP)"
#define AMIS_READER_KEY "Amis Reader Key"

#define SIM_ADDITIONAL_LOAD "SIM_Additional_Load"
#define FORCE_HEIZPATRONE "Force Heizpatrone"
// getOverview
#define WWW_FRONIUS "FR"
#define WWW_FRONIUS_IP "FRIP"
#define WWW_AMIS "AM"
#define WWW_AMIS_IP "AMIP"
#define WWW_CARDREADER "CR"
#define WWW_AKKU "AK"
#define WWW_AKKU_KAPA "AKK"
#define WWW_FLASH "FL"
#define WWW_INFLUX "IN"
#define WWW_INFLUX_IP "INIP"
#define WWW_MODBUS "MB"
#define WWW_MODBUS_IP "MBIP"
#define WWW_MQTT "MQ"
#define WWW_MQTT_IP "MQIP"
#define WWW_TEMP_SENSOR "TEMP"
#define WWW_TEMP_SENSOR_VAL "TEMPV"

void ajaxCalls_init(CALLBACK_GET_DATA webSockData, CALLBACK_SET_SETUP_CHANGED setupCh);
void ajaxCalls_handleGetSetup(AsyncWebServerRequest *request);
void ajaxCalls_handleGetOverview(AsyncWebServerRequest *request);
void ajaxCalls_handleStoreSetup(AsyncWebServerRequest *request, JsonVariant &json, bool isAPModus);