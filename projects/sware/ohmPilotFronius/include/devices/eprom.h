#pragma once

#include "defines.h"

#define CREDENTIALS "credentials"
#define _SSID "ssid"
#define _PASSWORD "pw"
#define _HYSTERESE "hy"
#define _INVERTER_IP "ip"
#define _AUSSCHALT_TEMP "ausT"
#define _EXTERNER_SPEICHER "eS"
#define _EXTERNER_SPEICHER_PRIORI "esP"
#define _PID_P "pid_p"
#define _PID_I "pid_i"
#define _PID_D "pid_d"
#define _PID_DIG_OUT_ON_DELAY_MS "doD"   // power has to be above set point before controller is invoked
#define _PID_DIG_OUT_OFF_DELAY_MS "ddms" // minimal time before pid controller turns off channel/pin
#define _PID_MIN_ON_TIME_MS "dmin"       // minimal on time for digital output (in ms)
#define _PID_TARGET_POWER "tpow"         // target power - max available power

void eprom_storeSetup(Setup &setup);
void eprom_getSetup(Setup &setup);

void eprom_test_write_Eprom(const char *, const char *);
void eprom_test_read_Eprom();
