//
// Created by ferdinand on 20.12.23.
//
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <modbus/modbus.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json_tokener.h>
#include <json-c/json_object.h>
#include "energy_logger.h"

#define AMIS_VALUE_COUNT 2

double amisValues[AMIS_VALUE_COUNT];

keysToValueMap_t amisKeyValueMap[AMIS_VALUE_COUNT] = {
        {"1.8.0", 0},
        {"2.8.0", 1}
};

#define REST_TARGET_COUNT 1
restTarget_t restTarget[REST_TARGET_COUNT] = {
        {"Amis reader", "amisreader", {0}, 80,
         "/rest", "GET /rest HTTP/1.0\r\n\r\n", -1,
         AMIS_VALUE_COUNT, amisValues, amisKeyValueMap}
};

#define RESPONSE_LENGTH 1024

// SYMO 5 inverter
// base values
#define SYMO5_INVERTER_REGS_COUNT 3
#define SYMO5_VALUES_COUNT 1

// lifetime energy values
#define SYMO5_INVERTER_REGS_COUNT2 4
#define SYMO5_VALUES_COUNT2 1

#define SYMO5_INVERTER_BLOCK_COUNT 2

// GEN24 inverter with associated smart meter
// base values
#define GEN24_INVERTER_REGS_COUNT 3
#define GEN24_VALUES_COUNT 1

// lifetime energy values
#define GEN24_INVERTER_REGS_COUNT2 4
#define GEN24_VALUES_COUNT2 1

#define GEN24_INVERTER_BLOCK_COUNT 2

// smart meter values
#define GEN24_METER_REGS_COUNT 17
#define GEN24_METER_VALUES_COUNT 8

#define GEN24_METER_BLOCK_COUNT 1

// SDM72D-M smart meter
// import/export values
#define SDM72_IMP_EXP_REGS_COUNT 4
#define SDM72_IMP_EXP_VALUES_COUNT 2

// total energy values
#define SDM72_TOTAL_ENERGY_REGS_COUNT 55
#define SDM72_TOTAL_ENERGY_VALUES_COUNT 5

// SDM72D-M instances: house, charger, mobile (test) unit
#define SDM72_HOUSE_BLOCK_COUNT 2
#define SDM72_CHARGER_BLOCK_COUNT 2
#define SDM72_MOBILE_BLOCK_COUNT 2

// modbus device id of inverters (symo 5, gen24)
#define INVERTER_ID 1
// modbus device id of the smart meter (accessed through gen24 via Modbus/RTU)
#define METER_ID 200
// modbus device id of the smart meter connected to the house
#define SDM_HOUSE_ID   1
// modbus device id of the smart meter connected to the charging station
#define SDM_CHARG_ID 2
// modbus device id of the smart meter connected to the mobile (test) metering box
#define SDM_MOBILE_ID 3

// number of devices within each Modbus/TCP target (Symo 5, GEN24, rtu2Eth converter)
#define SYMO5_DEVICE_COUNT 1
#define GEN24_DEVICE_COUNT 2
#define ETH_RTU_DEVICE_COUNT 3

// number of modbus/TCP targets
#define MODBUS_TARGET_COUNT 3 // 3 for Symo 5, GEN24, and EthRtu


// complete modbus target, device registers, ... structure
uint16_t symo5InverterRegs[SYMO5_INVERTER_REGS_COUNT];
double symo5Values[SYMO5_VALUES_COUNT];
regsToValueMap_t symo5Mapping[SYMO5_VALUES_COUNT] = {{0, 2, 0, 2}};

uint16_t symo5InverterRegs2[SYMO5_INVERTER_REGS_COUNT2];
double symo5Values2[SYMO5_VALUES_COUNT2];
regsToValueMap_t symo5Mapping2[SYMO5_VALUES_COUNT2] = {{0, 4, 0, -1}};

modbus_regs_block_t symo5InverterBlocks[SYMO5_INVERTER_BLOCK_COUNT] = {
        // AC lifetime active (real) energy output in Wh (acc64)
        {MODBUS_FC_READ_HOLDING_REGISTERS, 40186, SYMO5_INVERTER_REGS_COUNT2,
                symo5InverterRegs2, SYMO5_VALUES_COUNT2,
                symo5Values2, symo5Mapping2},
        // AC lifetime energy production (may be imprecise) (acc32)
        {MODBUS_FC_READ_HOLDING_REGISTERS, 40093, SYMO5_INVERTER_REGS_COUNT,
                symo5InverterRegs,  SYMO5_VALUES_COUNT,
                symo5Values,  symo5Mapping}
};

uint16_t gen24InverterRegs[GEN24_INVERTER_REGS_COUNT];
double gen24Values[GEN24_VALUES_COUNT];
regsToValueMap_t gen24Mapping[GEN24_VALUES_COUNT] = {{0, 2, 0, 2}};

uint16_t gen24InverterRegs2[GEN24_INVERTER_REGS_COUNT2];
double gen24Values2[GEN24_VALUES_COUNT2];
regsToValueMap_t gen24Mapping2[GEN24_VALUES_COUNT2] = {{0, 4, 0, -1}};

modbus_regs_block_t gen24InverterBlocks[GEN24_INVERTER_BLOCK_COUNT] = {
        // C lifetime active (real) energy output in Wh (acc64)
        {MODBUS_FC_READ_HOLDING_REGISTERS, 40186, GEN24_INVERTER_REGS_COUNT2,
                gen24InverterRegs2, GEN24_VALUES_COUNT2,
                gen24Values2, gen24Mapping2},
        // AC energy (acc32)
        {MODBUS_FC_READ_HOLDING_REGISTERS, 40093, GEN24_INVERTER_REGS_COUNT,
                gen24InverterRegs,  GEN24_VALUES_COUNT,
                gen24Values,  gen24Mapping}
};

uint16_t gen24MeterRegs[GEN24_METER_REGS_COUNT];
double gen24MeterValues[GEN24_METER_VALUES_COUNT];
regsToValueMap_t gen24MeterMapping[GEN24_METER_VALUES_COUNT] = {
        {0,  2, 0, 16},
        {2,  2, 1, 16},
        {4,  2, 2, 16},
        {6,  2, 3, 16},
        {8,  2, 4, 16},
        {10, 2, 5, 16},
        {12, 2, 6, 16},
        {14, 2, 7, 16}
};

modbus_regs_block_t gen24MeterBlocks[GEN24_METER_BLOCK_COUNT] = {
        // total real energy exported (acc32), ...
        {MODBUS_FC_READ_HOLDING_REGISTERS, 40107,
         GEN24_METER_REGS_COUNT, gen24MeterRegs,
         GEN24_METER_VALUES_COUNT, gen24MeterValues,
         gen24MeterMapping}
};

uint16_t sdm72HouseMeterImpExpRegs[SDM72_IMP_EXP_REGS_COUNT];
double sdm72HouseMeterImpExpValues[SDM72_IMP_EXP_VALUES_COUNT];
regsToValueMap_t sdm72HouseMeterImpExpMapping[SDM72_IMP_EXP_VALUES_COUNT] = {
        {0, 2, 0, -1},
        {2, 2, 1, -1}
};

uint16_t sdm72HouseMeterEnergyRegs[SDM72_TOTAL_ENERGY_REGS_COUNT];
double sdm72HouseMeterTotalEnergyValues[SDM72_TOTAL_ENERGY_VALUES_COUNT];
regsToValueMap_t sdm72HouseMeterTotalEnergyMapping[SDM72_TOTAL_ENERGY_VALUES_COUNT] = {
        {0,  2, 0, -1},
        {42, 2, 1, -1},
        {46, 2, 2, -1},
        {48, 2, 3, -1},
        {54, 2, 4, -1}
};

modbus_regs_block_t sdm72HouseMeterBlocks[SDM72_HOUSE_BLOCK_COUNT] = {
        {MODBUS_FC_READ_INPUT_REGISTERS, 0x48,  SDM72_IMP_EXP_REGS_COUNT,      sdm72HouseMeterImpExpRegs,
                SDM72_IMP_EXP_VALUES_COUNT,      sdm72HouseMeterImpExpValues,
                sdm72HouseMeterImpExpMapping},
        {MODBUS_FC_READ_INPUT_REGISTERS, 0x156, SDM72_TOTAL_ENERGY_REGS_COUNT, sdm72HouseMeterEnergyRegs,
                SDM72_TOTAL_ENERGY_VALUES_COUNT, sdm72HouseMeterTotalEnergyValues,
                sdm72HouseMeterTotalEnergyMapping}
};

uint16_t sdm72ChargerMeterImpExpRegs[SDM72_IMP_EXP_REGS_COUNT];
double sdm72ChargerMeterImpExpValues[SDM72_IMP_EXP_VALUES_COUNT];
regsToValueMap_t sdm72ChargerMeterImpExpMapping[SDM72_IMP_EXP_VALUES_COUNT] = {
        {0, 2, 0, -1},
        {2, 2, 1, -1}
};

uint16_t sdm72ChargerMeterEnergyRegs[SDM72_TOTAL_ENERGY_REGS_COUNT];
double sdm72ChargerMeterTotalEnergyValues[SDM72_TOTAL_ENERGY_VALUES_COUNT];
regsToValueMap_t sdm72ChargerMeterTotalEnergyMapping[SDM72_TOTAL_ENERGY_VALUES_COUNT] = {
        {0,  2, 0, -1},
        {42, 2, 1, -1},
        {46, 2, 2, -1},
        {48, 2, 3, -1},
        {54, 2, 4, -1}
};

modbus_regs_block_t sdm72ChargerMeterBlocks[SDM72_CHARGER_BLOCK_COUNT] = {
        // import active energy (float32), ...
        {MODBUS_FC_READ_INPUT_REGISTERS, 0x48,  SDM72_IMP_EXP_REGS_COUNT,      sdm72ChargerMeterImpExpRegs,
                SDM72_IMP_EXP_VALUES_COUNT,      sdm72ChargerMeterImpExpValues,
                sdm72ChargerMeterImpExpMapping},
        // total active energy (float32), ...
        {MODBUS_FC_READ_INPUT_REGISTERS, 0x156, SDM72_TOTAL_ENERGY_REGS_COUNT, sdm72ChargerMeterEnergyRegs,
                SDM72_TOTAL_ENERGY_VALUES_COUNT, sdm72ChargerMeterTotalEnergyValues,
                sdm72ChargerMeterTotalEnergyMapping}
};

uint16_t sdm72MobileMeterImpExpRegs[SDM72_IMP_EXP_REGS_COUNT];
double sdm72MobileMeterImpExpValues[SDM72_IMP_EXP_VALUES_COUNT];
regsToValueMap_t sdm72MobileMeterImpExpMapping[SDM72_IMP_EXP_VALUES_COUNT] = {
        {0, 2, 0, -1},
        {2, 2, 1, -1}
};

uint16_t sdm72MobileMeterEnergyRegs[SDM72_TOTAL_ENERGY_REGS_COUNT];
double sdm72MobileMeterTotalEnergyValues[SDM72_TOTAL_ENERGY_VALUES_COUNT];
regsToValueMap_t sdm72MobileMeterTotalEnergyMapping[SDM72_TOTAL_ENERGY_VALUES_COUNT] = {
        {0,  2, 0, -1},
        {42, 2, 1, -1},
        {46, 2, 2, -1},
        {48, 2, 3, -1},
        {54, 2, 4, -1}
};

modbus_regs_block_t sdm72MobileMeterBlocks[SDM72_CHARGER_BLOCK_COUNT] = {
        {MODBUS_FC_READ_INPUT_REGISTERS, 0x48,  SDM72_IMP_EXP_REGS_COUNT,      sdm72MobileMeterImpExpRegs,
                SDM72_IMP_EXP_VALUES_COUNT,      sdm72MobileMeterImpExpValues,
                sdm72MobileMeterImpExpMapping},
        {MODBUS_FC_READ_INPUT_REGISTERS, 0x156, SDM72_TOTAL_ENERGY_REGS_COUNT, sdm72MobileMeterEnergyRegs,
                SDM72_TOTAL_ENERGY_VALUES_COUNT, sdm72MobileMeterTotalEnergyValues,
                sdm72MobileMeterTotalEnergyMapping}
};

device_t symo5Device[SYMO5_DEVICE_COUNT] = {
        {INVERTER_ID,
         "SYMO5",
         "symo5.log",
         NULL,
         SYMO5_INVERTER_BLOCK_COUNT,
         symo5InverterBlocks}
};

device_t gen24Device[GEN24_DEVICE_COUNT] = {
        {INVERTER_ID,
                "GEN24",
                "gen24.log",
                NULL,
                GEN24_INVERTER_BLOCK_COUNT,
                gen24InverterBlocks},
        {METER_ID,
                "METER",
                "meter.log",
                NULL,
                GEN24_METER_BLOCK_COUNT,
                gen24MeterBlocks}
};

// TODO: reduction of mobile to 1 instance and activation of house and charger
device_t ethRtuDevice[ETH_RTU_DEVICE_COUNT] = {
//        {SDM_HOUSE_ID,  "House", "house.log", NULL,   SDM72_HOUSE_BLOCK_COUNT,   sdm72HouseMeterBlocks},
        {SDM_MOBILE_ID, "House",   "house.log",   NULL, SDM72_MOBILE_BLOCK_COUNT, sdm72MobileMeterBlocks},
//        {SDM_CHARG_ID,  "Charger", "charger.log", NULL, SDM72_CHARGER_BLOCK_COUNT, sdm72ChargerMeterBlocks},
        {SDM_MOBILE_ID, "Charger", "charger.log", NULL, SDM72_MOBILE_BLOCK_COUNT, sdm72MobileMeterBlocks},
        {SDM_MOBILE_ID, "Mobile",  "mobile.log",  NULL, SDM72_MOBILE_BLOCK_COUNT, sdm72MobileMeterBlocks},
};

target_t modbusTcpTarget[MODBUS_TARGET_COUNT] = {
        {"Symo-5", "10.0.0.17", 502, NULL, SYMO5_DEVICE_COUNT,   symo5Device},
        {"GEN24",  "10.0.0.13", 502, NULL, GEN24_DEVICE_COUNT,   gen24Device},
        {"EthRtu", "10.0.0.30", 502, NULL, ETH_RTU_DEVICE_COUNT, ethRtuDevice}
};

// 60 time offset @ Raspberry Pi Zero
config_t config = {MODBUS_ERROR_RECOVERY_NONE, 3600, MODBUS_TARGET_COUNT, modbusTcpTarget, VERBOSITY_INFO};

// name of energy logger's configuration file
#define ENERGY_MGR_CONF_FILE_NAME "energy-logger.conf"

// time stamp as name
char timeNowText[24];
char timeNowLogText[24];
char logText[48];

// initialize the rest API targets
int initRestTargets(restTarget_t target[], int targetCount) {
    for (int i = 0; i < targetCount; i++) {
        struct hostent *server = gethostbyname(target[i].hostname);
        if (server != NULL) {
            memset(&target[i].serverAddr, 0, sizeof(target[i].serverAddr));
            target[i].serverAddr.sin_family = AF_INET;
            target[i].serverAddr.sin_port = htons(target[i].port);
            memcpy(&target[i].serverAddr.sin_addr, server->h_addr, server->h_length);
        }
    }
}

// read rest API targets
int readRestTargets(restTarget_t target[], int targetCount) {
    for (int i = 0; i < targetCount; i++) {
        target[i].socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (target[i].socketFd >= 0) {
            int retVal = connect(target[i].socketFd, (struct sockaddr *) &target[i].serverAddr,
                                 sizeof(target[i].serverAddr));
            if (retVal >= 0 || errno == EISCONN) {
                readJsonResponse(&target[i]);
                close(target[i].socketFd);
            }
        }
    }
}

int readJsonResponse(restTarget_t *target) {
    int bytes, sent, received, total;
    char response[RESPONSE_LENGTH];
    int responseLen = sizeof(response);
    char *jsonStart;

/* send the request */
    total = strlen(target->request);
    sent = 0;
    do {
        bytes = write(target->socketFd, target->request + sent, total - sent);
        if (bytes < 0)
            return bytes;
        if (bytes == 0)
            break;
        sent += bytes;
    } while (sent < total);

/* receive the response */
    memset(response, 0, responseLen);
    total = responseLen - 1;
    received = 0;
    do {
        bytes = read(target->socketFd, response + received, total - received);
        if (bytes < 0)
            return bytes;
        if (bytes == 0)
            break;
        received += bytes;
    } while (received < 1);//total);

    if (received == total)
        return 0;
    jsonStart = strchr(response, '{');
    if (jsonStart != NULL) {
        mapJsonValues(target, jsonStart);
    }
    return 1;
}

void mapJsonValues(restTarget_t *target, char jsonString[]) {
    json_object *objPtr;
    json_object *obj = json_tokener_parse(jsonString);
    for (int i = 0; i < target->valueCount; i++) {
        json_object_object_get_ex(obj, target->mapping[i].key, &objPtr);
        target->values[i] = json_object_get_double(objPtr);
//        printf("%s\t%.3lf\t", target->mapping[i].key, target->values[i]);
//        fflush(stdout);
    }
//    printf("\n");
}


int main(int argc, char *argv[]) {

    // return code, number of retryCount, retries limit, wait time, ...
    int rc;

    // current timestamp
    struct tm timeNow;
    // hour of most recent evaluation/change
    int prevHour;
    // length of one cycle in minutes
    int minutesPerCycle = 1;
    // delay between retries in minutes
    int retryDelayMinutes = 5;
    // minutes to wait
    int minutesToWait;
    // double value as string
    char text[11][16];
    // column separator symbol (tab, semicolon, ...)
    char colSep = '\t';
    // flush text to file
    bool doFlush;

    // for debugging purposes
    config_t *configPtr = &config;

    // set the INT (ctrl-c) and TERM signal handler to 'catchIntSignal'
    signal(SIGINT, catchIntSignal);
    signal(SIGTERM, catchIntSignal);
    signal(SIGABRT, catchIntSignal);

    // evaluate command line arguments
    //evalArgs(argc, argv, &config);

    // get current time
    refreshTimeNow(&timeNow, config.timeOffset, timeNowText);

    // initialize rest API targets
    initRestTargets(restTarget, REST_TARGET_COUNT);
    // create modbus object for modbus/tcp targets
    for (int i = 0; i < config.targetCount; i++) {
        config.target[i].target = modbus_new_tcp(config.target[i].ip, config.target[i].port);
        if (config.target[i].target == NULL) {
            if (config.verbosityLevel >= VERBOSITY_ERROR) {
                fprintf(stderr, "%s: Unable to initialize Modbus/TCP slave %s at port %d\n", timeNowText,
                        config.target[i].ip,
                        config.target[i].port);
            }
            return -1;
        }
        // set response timeout to 5.2 seconds
        modbus_set_response_timeout(config.target[i].target, 5, 200000);
        // set reconnect on failure bits, if set in command line arguments
        modbus_set_error_recovery(config.target[i].target, config.errorRecoveryMode);
        // connect to inverter
        rc = modbus_connect(config.target[i].target);
        if (rc == -1) {
            if (config.verbosityLevel >= VERBOSITY_ERROR) {
                fprintf(stderr, "%s: Connect failed: %s\n", timeNowText, modbus_strerror(errno));
            }
            modbus_close(config.target[i].target); // close failed connection??
            // continue and retryCount all the time
            //modbus_free(config.target[i].target); // stay here, don't leave
            //return -2;
        }
        for (int j = 0; j < config.target[i].deviceCount; j++) {
            config.target[i].device[j].file = fopen(config.target[i].device[j].filename, "a");
            if (config.target[i].device[j].file == NULL) {
                fprintf(stderr, "%s: Opening log file '%s' failed: %s\n", timeNowText,
                        config.target[i].device[j].filename, strerror(errno));
            }
        }
    }

    // import configuration from config file
    //readConfigFile(&config);

    // draw column header
//    printf("Time;SYMO 5;SYMO LT;GEN24;GEN24 LT;Export;Exp A;Exp B;Exp C;Imp;Imp A;Imp B;Imp C"
//           ";Mobile Imp;Mobile Exp;Mobile Tot;Reset Imp;Reset Exp;Reset Tot;Net kWh"
//           ";House Imp;House Exp;House Tot;Reset Imp;Reset Exp;Reset Tot;Net kWh"
//           ";Charger Imp;Charger Exp;Charger Tot;Reset Imp;Reset Exp;Reset Tot;Net kWh\n");
//    printf("Datum;kWh;kW;Status;Symo5;Symo5;GEN24;GEN24;EXPORT;IMPORT;IMPORT;EXPORT;SALDO\n");
    printf("Datum%ckWh%ckW%cStatus%cSymo5%cGEN24%cIMPORT%cEXPORT%cIMPORT%cEXPORT%cIMPORT%cEXPORT%cSALDO\n",
           colSep, colSep, colSep, colSep, colSep, colSep, colSep, colSep, colSep, colSep, colSep, colSep);
    do {
        // get current time and set minute of the day
        refreshTimeNow(&timeNow, config.timeOffset, timeNowText);
        if (timeNow.tm_min % 15 == 0) { // every quarter of an hour
            doFlush = timeNow.tm_min < 15;
//        printf("%s;", timeNowText);
            fillTimeNowLogText(timeNowLogText, timeNow);
            printf("%s%c0,0%c0,0%cVALID%c", timeNowLogText, colSep, colSep, colSep, colSep);
            // read modbus/tcp targets
            for (int i = 0; i < config.targetCount; i++) {
                rc = readTargetRetrying(&config.target[i], retryDelayMinutes, timeNowText, colSep, doFlush);
            }
            readRestTargets(restTarget, REST_TARGET_COUNT);
            printf("%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s\n"/*"%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f\n"*/,
                   formatDouble(text[0], config.target[0].device[0].block[0].values[0]),
                   colSep, // SYMO 5 AC lifetime energy
//                   formatDouble(text[1],
//                                config.target[0].device[0].block[1].values[0]),
//                   colSep, // SYMO 5 real AC lifetime energy
                   formatDouble(text[2], config.target[1].device[0].block[0].values[0]),
                   colSep, // GEN24 AC lifetime energy
//                   formatDouble(text[3],
//                                config.target[1].device[0].block[1].values[0]), colSep, // GEN24 AC real lifetime energy
                   formatDouble(text[4],
                                config.target[1].device[1].block[0].values[4]),
                   colSep, // GEN24 smart meter total energy imported
                   formatDouble(text[5],
                                config.target[1].device[1].block[0].values[0]),
                   colSep, // GEN24 smart meter total energy exported
                   formatDouble(text[6], restTarget[0].values[0]),
                   colSep, // amis reader total energy imported
                   formatDouble(text[7], restTarget[0].values[1]),
                   colSep, // amis reader total energy exported
                   formatDouble(text[8],
                                config.target[2].device[0].block[0].values[0]),
                   colSep, // SDM72D-M total energy imported
                   formatDouble(text[9],
                                config.target[2].device[0].block[0].values[1]),
                   colSep, // SDM72D-M total energy exported
                   formatDouble(text[10], // SDM72D-M net energy (import - export)
                                config.target[2].device[0].block[1].values[4])
            );
            fflush(stdout);
        }
//        if (timeNow.tm_sec % 10 == 0) {
//            readRestTargets(restTarget, REST_TARGET_COUNT);
//        }

        waitForMinutes(minutesPerCycle);
        // get current time and calculate the minute of the day
        /*minuteNow = */refreshTimeNow(&timeNow, config.timeOffset, timeNowText);
        //minuteNow = timeNow.tm_hour * 60 + timeNow.tm_min;
    } while (true);
    return 0;
} // main

int closeTarget() {
    // shut down modbus connections
    for (int i = 0; i < config.targetCount; i++) {
        if (config.target[i].target != NULL) {
            modbus_close(config.target[i].target);
            modbus_free(config.target[i].target);
            for (int j = 0; j < config.target[i].deviceCount; j++) {
                if (config.target[i].device[j].file != NULL) {
                    fclose(config.target[i].device[j].file);
                }
            }
        }
    }
    return 0;
}

int readTargetRetrying(target_t *target, int retryDelayMinutes, char timeNowText[], char colSep, bool doFlush) {
    // return code, number of retryCount, retries limit, wait time, ...
    int rc;
    int retryCount;
    int maxRetries = 5;

    // try to read inverterBlocks from inverter at end of daily PV production period
    rc = readTarget(target, timeNowText, colSep, doFlush);
    retryCount = 0;
    while (rc != 0 && retryCount < maxRetries) {
        retryCount++;
        waitForMinutes(retryDelayMinutes); // try again in 5 minutes
        rc = reconnect(target->target, timeNowText); // read(s) failed, thus reconnect
        if (rc == 0) {
            rc = readTarget(target, timeNowText, colSep, doFlush);
        }
    }
    return rc;
}

void waitForMinutes(int minutes) {
    if (minutes > 0) {
        unsigned int seconds = minutes * 60;
        sleep(seconds);
    }
}


// refresh current time and add time offset
int refreshTimeNow(struct tm *tm, long timeOffset, char text[]) {
    time_t T;
    time(&T);
    T += timeOffset; // add time offset given as command line argument (unifi pi: 1 hour too late, GMT!)
    *tm = *localtime(&T);
    // build name from time stamp inverterBlocks
    fillTimeNowText(text, *tm);
    return calcMinuteNow(*tm);
}


// fill timestamp string
void fillTimeNowText(char timeStamp[], struct tm tm) {
    sprintf(timeStamp,
            "%04d-%02d-%02d %02d:%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// fill timestamp string
void fillTimeNowLogText(char timeStamp[], struct tm tm) {
    // for 1/4 hour log:
    // format: dd.mm.yyyy hh:mm;0,0;0,0;[IN]VALID;integral,partial
    // example: 31.01.2023 17:00;0,0;0,0;INVALID;3648,049
    sprintf(timeStamp,
            "%02d.%02d.%04d %02d:%02d",
            tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
}

int calcMinuteNow(struct tm tm) {
    return (tm.tm_hour * 60 + tm.tm_min) % (24 * 60);
}

// the signal handler for CTRL+C
void catchIntSignal(int sig_num) {
// reinstall the signal handler for CTRL+C
    signal(SIGINT, catchIntSignal);
    signal(SIGTERM, catchIntSignal);
    signal(SIGABRT, catchIntSignal);
    if (sig_num == SIGTERM || sig_num == SIGINT || sig_num == SIGABRT) {
        closeTarget();
        fputs("Terminated by CTRL+c or kill SIGTERM\n", stderr);
    }
}

int writeLogLine(device_t *device, char colSep, bool doFlush) {
    char text[20];
    if (device->file != NULL) {
        fprintf(device->file, "%s%c0,0%c0,0%cVALID%c%s\n", timeNowLogText, colSep, colSep, colSep, colSep,
                formatDouble(text, device->block[0].values[0]));
        if (doFlush) {
            fflush(device->file);
        }
    }
}

int readTarget(target_t *target, char timeStamp[], char colSep, bool doFlush) {
    int rc;
    // invertr successfully read?
    bool readSuccess = true;

    for (int i = 0; i < target->deviceCount; i++) {
        // use modbus unit INVERTER
        rc = modbus_set_slave(target->target, target->device[i].deviceId);
        if (rc == -1) {
            fprintf(stderr, "%s: Inverter select failed: %s\n", timeStamp,
                    modbus_strerror(errno));
        } else {
            // read all desired blocks from device
            for (int j = 0; j < target->device[i].blockCount; j++) {
                // distiguish between holding register (fronius equipment)
                // and input registers (eastron SDM72D-M)
                switch (target->device[i].block->regType) {
                    case MODBUS_FC_READ_HOLDING_REGISTERS:
                        rc = modbus_read_registers(target->target, target->device[i].block[j].baseAddr,
                                                   target->device[i].block[j].regCount,
                                                   (uint16_t *) target->device[i].block[j].reg);
                        break;
                    case MODBUS_FC_READ_INPUT_REGISTERS:
                        rc = modbus_read_input_registers(target->target, target->device[i].block[j].baseAddr,
                                                         target->device[i].block[j].regCount,
                                                         (uint16_t *) target->device[i].block[j].reg);
                        break;
                    default:
                        rc = 0;
                }
                if (rc == -1) {
                    fprintf(stderr, "%s: Target %s (%s) / Device %s (%d) read failed: %s\n", timeStamp,
                            target->text, target->ip, target->device[i].name,
                            target->device[i].deviceId, modbus_strerror(errno));
                    readSuccess = false;
                } else if (rc > 0) {
                    mapValues(&target->device[i].block[j]);
                    //showValues(&target->device[i].block[j]);
                }

// in case of successful read operations
                if (readSuccess) {
                    rc = 0;
                } else {
                    rc = -2; // read failure
                }
            }
            // write one log line per device
            writeLogLine(&target->device[i], colSep, doFlush);
        }
    }
    return rc; // 0 ok, -1 device select error, -2 read error
}

int mapValues(modbus_regs_block_t *block) {
    for (int i = 0; i < block->valueCount; i++) {
        switch (block->mapping[i].regCount) {
            case 1:
                block->values[block->mapping[i].valueIndex] =
                        block->reg[block->mapping[i].regIndex];
                break;
            case 2:
                if (block->mapping[i].scaleFactorIndex == -1) {
                    block->values[block->mapping[i].valueIndex] = modbus_get_float_abcd(
                            &block->reg[block->mapping[i].regIndex]);
                } else {
                    block->values[block->mapping[i].valueIndex] =
                            (unsigned long long) block->reg[block->mapping[i].regIndex] << 16 |
                            (unsigned long long) block->reg[block->mapping[i].regIndex + 1];
                }
                break;
            case 4:
                block->values[block->mapping[i].valueIndex] =
                        (unsigned long long) block->reg[block->mapping[i].regIndex] << 48 |
                        (unsigned long long) block->reg[block->mapping[i].regIndex + 1] << 32 |
                        (unsigned long long) block->reg[block->mapping[i].regIndex + 2] << 16 |
                        (unsigned long long) block->reg[block->mapping[i].regIndex + 3];
                break;
            default:
                block->values[block->mapping[i].valueIndex] = 0;
        }
        if (block->mapping[i].scaleFactorIndex >= 0) {
            block->values[block->mapping[i].valueIndex] *=
                    pow(10, (int16_t) block->reg[block->mapping[i].scaleFactorIndex]);
        }
    }
    return 0;
}

int showValues(modbus_regs_block_t *block, char colSep) {
    for (int i = 0; i < block->valueCount; i++) {
        printf("%.3lf%c", block->values[i], colSep);
    }
    return 0;
}

char *formatDouble(char text[], double value) {
    long long intVal = (long long) value;
    int realVal = abs((int) ((value - intVal) * 1000));
    sprintf(text, "%lld,%03d", intVal, realVal);
    return text;
}

// reconnect to server
int reconnect(modbus_t *target, char timeStamp[]) {
// connect to target
    int rc = modbus_connect(target);
    if (rc == -1) {
        fprintf(stderr, "%s: Connect failed: %s\n", timeStamp, modbus_strerror(errno));
        modbus_close(target); // close failed connection??
    }
    return rc;
}