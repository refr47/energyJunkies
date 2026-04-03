#pragma once

/* Reihenfolge ist wichtig */
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncJson.h>

#include "debugConsole.h"
#include "defines.h"

#define WLAN_ESSID "wlan_ssid"
#define WLAN_PASSWD "wlan_password"
#define IP_INVERTER "ip_wechselrichter"
#define HEIZSTABLEISTUNG "heizstab_leistung"
#define LEGIONELLEN_DELTA_TIME "legionellen_differenz"
#define LEGIONELLEN_TEMP "legionellen_temp"
#define EXTERNER_SPEICHER "akku_vorhanden"
#define EXTERNER_SPEICHER_PRIORI "akku_priori"
#define TEMP_AUSSCHALTEN "heizstab_temp_min"
#define TEMP_EINSCHALT "heizstab_temp_max"
 
#define WWW_MQTT_HOST "mqtt_server_ip"
#define WWW_MQTT_USER "mqtt_user"
#define WWW_MQTT_PASWWD "mqtt_passwd"

#define WWW_INFLUX_HOST "influx_server_ip"
#define WWW_INFLUX_TOKEN "influx_token"
#define WWW_INFLUX_ORG "influg_org"
#define WWW_INFLUX_BUCKET "influx_bucket"

#define AMIS_READER_HOST "amisreader_ip"
#define AMIS_READER_KEY "amisreader_key"

#define FORCE_HEIZPATRONE "heizstab_force"

/* getOverview */
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
#define WWW_EPSILON "EpsilonPin"


void ajaxCalls_init(CALLBACK_GET_DATA getData, CALLBACK_SET_SETUP_CHANGED setupCh);

void ajaxCalls_handleGetSetup(AsyncWebServerRequest *request);
void ajaxCalls_handleGetOverview(AsyncWebServerRequest *request);
void ajaxCalls_handleStoreSetup(DynamicJsonDocument &json, AsyncWebServerRequest *request, bool isAPModus);

/* Shelly:
 * 1) ajaxCalls_triggerShellyScan() startet RTOS-Task
 * 2) ajaxCalls_handleBuildAndGetShelly() liefert letzten Snapshot zurück
 */
bool ajaxCalls_triggerShellyScan(void);
void ajaxCalls_handleBuildAndGetShelly(AsyncWebServerRequest *request);