#pragma once

#define WLAN_ESSID "WLAN_ESSID"
#define WLAN_PASSWD "WLAN_Password"
#define IP_INVERTER "IP_Inverter"
#define HYSTERESE "Hysterese"
#define EINSPEISUNG_MUSS "Einspeisung"
#define MINDEST_LAUFZEIT_DIGITALER_OUT "Mindeslaufzeit_Digital"
#define MINDEST_LAUFZEIT_PORT_ON "Mindeslaufzeit_Phase"
#define MINDEST_LAUFZEIT_REGLER_KONSTANT "Mindeslaufzeit_Regler"
#define EXTERNER_SPEICHER "Externer_Speicher"
#define EXTERNER_SPEICHER_PRIORI "Prioritaet"
#define AUSSCHALT_TEMP "Ausschalt_Temperatur"
#define PID_P "Ausgangsregler (P-Anteil)"
#define PID_I "Ausgangsregler (I-Anteil)"
#define PID_D "Ausgangsregler (D-Anteil)"

#include "debugConsole.h"

void www_init(char *ipAddr, char *); // IF NULL, ACT as Access Point
void www_run();
