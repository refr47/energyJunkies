#pragma once
/* Reihenfolge ist wichtig - ansonst fehler from rtos*/
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include <AsyncJson.h>
#include "debugConsole.h"

#define WLAN_ESSID "WLAN_ESSID"
#define WLAN_PASSWD "WLAN_Password"
#define IP_INVERTER "IP_Inverter"
#define HEIZSTABLEISTUNG "Heizstableistung"
#define EINSPEISUNG_MUSS "Mindest_Einspeisung"
#define MINDEST_LAUFZEIT_DIGITALER_OUT "Mindeslaufzeit_Digital"
#define MINDEST_LAUFZEIT_PORT_ON "Mindeslaufzeit_Phase"
#define MINDEST_LAUFZEIT_REGLER_KONSTANT "Mindeslaufzeit_Regler"
#define EXTERNER_SPEICHER "Speicher"
#define EXTERNER_SPEICHER_PRIORI "Speicher_Prioritaet"
#define TEMP_AUSSCHALTEN "Ausschalt_Temperatur"
#define TEMP_EINSCHALT "Einschalt_Temperatur"
#define PID_P "Ausgangsregler (P-Anteil)"
#define PID_I "Ausgangsregler (I-Anteil)"
#define PID_D "Ausgangsregler (D-Anteil)"

#define SIM_ADDITIONAL_LOAD "SIM_Additional_Load"
#define SIM_BIAS_POWER "SIM_Bias_Powery"

void ajaxCalls_handleGetSetup(AsyncWebServerRequest *request);
void ajaxCalls_handleStoreSetup(AsyncWebServerRequest *request, JsonVariant &json, bool isAPModus);