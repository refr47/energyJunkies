#ifndef __DEFINES__H
#define __DEFINES__H

#define LEN_WLAN 30
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
    bool externerSpeicher;
    String ipInverterAsString;
    float pid_p;
    float pid_i;
    float pid_d;

} Setup;

#define MODBUS_PORT "502"

#endif