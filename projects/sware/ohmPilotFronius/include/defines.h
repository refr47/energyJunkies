#pragma once

#define LEN_WLAN 30
#define MODBUS_PORT "502"
#define JSON_OBJECT_SETUP_LEN 500 /// www.cpp,
#include "debugConsole.h"

typedef struct
{
    char ssid[LEN_WLAN];
    char passwd[LEN_WLAN];

    unsigned int regelbereichHysterese;
    unsigned int ausschaltTempInGradCel;
    unsigned int ipInverter;
    String ipInverterAsString;
    bool externerSpeicher;
    char externerSpeicherPriori;
    float pid_p;
    float pid_i;
    float pid_d;
    unsigned int pid_min_time_without_contoller_inMS;
    unsigned int pid_min_time_before_switch_off_channel_inMS;
    unsigned int pid_min_time_for_dig_output_inMS;
    unsigned int pid_targetPowerInWatt; // Wieviel müss übrig bleiben
    bool pidChanged;
} Setup;
