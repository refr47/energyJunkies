#pragma once
#include "defines.h"
// Frozen binary layout of schema 0/1. Never extend these structures.
typedef struct {
    char influxHost[51];
    char influxToken[96];
    char influxOrg[31];
    char influxBucket[31];
} LegacyInfluxV1;


typedef struct {
    char mqttHost[51];
    char mqttUser[26];
    char mqttPass[26];
} LegacyMqttV1;


typedef struct
{
    char ssid[31];
    char passwd[31];

    unsigned int heizstab_leistung_in_watt;
    unsigned int phasen_leistung_in_watt; // heizstab_leistung_in_watt  pre calculation #define LOG_BUFFER_SIZE 256@see: eprom_getSetup
    unsigned int tempMaxAllowedInGrad;
    unsigned int tempMinInGrad;
    // unsigned int ipInverter;
    char inverter[17];
    char currentIP[17];

    bool akku;
    short akkuPriori; // 1 oder 2, je nachdem, ob Akku vorrangig behandelt werden soll oder nicht
    // unsigned int pid_min_time_without_contoller_inMS;
    unsigned int legionellenDelta;
    unsigned int legionellenMaxTemp;

    LegacyInfluxV1 influxSetup;
    LegacyMqttV1 mqttSetup;
    // unsigned int pid_powerWhichNeedNotConsumed; // Wieviel müss übrig bleiben
    //  bool pidChanged;
    //  unsigned int ipAmisReaderHost;
    char amisKey[36];
    char amisReaderHost[17];

    //char loxoneHost[17];




    double epsilonML_PinManager;
    int forceHeating;
    short forceHeatingStop;
    unsigned int wattSetupForTest;
    bool setupChanged;

} LegacySetupV1;
