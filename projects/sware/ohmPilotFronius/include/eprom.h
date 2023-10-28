#ifndef __EEPROM_H
#define __EEPROM_H

#include "defines.h"

#define CREDENTIALS "credentials"
#define _SSID "ssid"
#define _PASSWORD "pw"
#define _HEIZPATRONE "hp"
#define _HYSTERESE "hy"
#define _EINSPEISEBESCHRAENKUNG "eb"
#define _MINDESTLAUFZEIT "ml"
#define _AUSSCHALT_TEMP "ausT"
#define _INVERTER_IP "invertIP"
#define _EXTERNER_SPEICHER "eS"
#define _PID_P "pid_p"
#define _PID_I "pid_i"
#define _PID_D "pid_d"

void eprom_storeSetup(Setup &setup);
void eprom_getSetup(Setup &setup);

void eprom_test_write_Eprom(const char *, const char *);
void eprom_test_read_Eprom();

#endif