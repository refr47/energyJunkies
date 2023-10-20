#ifndef __EEPROM_H
#define __EEPROM_H

#define CREDENTIALS "credentials"
#define _SSID "ssid"
#define _PASSWORD "password"
#define _HEIZPATRONE "heizpatrone"
#define _HYSTERESE "hysterese"
#define _EINSPEISEBESCHRAENKUNG "einspeisebeschraenkung"
#define _MINDESTLAUFZEIT "mindestLaufzeit"
#define _AUSSCHALT_TEMP "ausschaltTemp"
#define _INVERTER_IP "inverterIP"
#define _PID_P "pid_p"
#define _PID_I "pid_i"
#define _PID_D "pid_d"

#define LEN_WLAN 20

typedef struct
{
    char ssid[LEN_WLAN];
    char passwd[LEN_WLAN];
    unsigned int leistungHeizpatroneInW;
    unsigned int regelbereichHysterese;
    unsigned int einspeiseBeschraenkingInW;
    unsigned int mindestLaufzeitInMin;
    unsigned int ausschaltTempInGradCel;
    unsigned int ipInverter;
    String ipInverterAsString;
    float pid_p;
    float pid_i;
    float pid_d;

} Setup;

void eprom_storeSetup(Setup &setup);
void eprom_getSetup(Setup &setup);

void eprom_test_write_Eprom(const char *, const char *);
void eprom_test_read_Eprom();

#endif