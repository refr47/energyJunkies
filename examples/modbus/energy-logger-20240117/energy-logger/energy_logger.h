//
// Created by ferdinand on 20.12.23.
//

#ifndef ENERGY_LOGGER_ENERGY_LOGGER_H
#define ENERGY_LOGGER_ENERGY_LOGGER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

// max. length of modbus target/device names
#define TARGET_NAME_LEN 24  //Modbus/TCP target object (inverter, rtu2Eth converter, ...)
#define DEVICE_NAME_LEN 24  //Modbus/TCP device object (inverter, smart meter, ...)
#define FILENAME_LEN 64 // maximum number of characters for log file name

// length of ip address string
#define IP_LENGTH 64
// host name length
#define HOSTNAME_LENGTH 64
// length of the resource identifier
#define RESOURCE_LENGTH 64
// HTTP rest API request length
#define REQUEST_LENGTH 64
// length of json object key
#define KEY_LENGTH 32

typedef struct {
    // (start) index of source register
    char key[KEY_LENGTH];
    // index of destination field element
    int valueIndex;
} keysToValueMap_t;

// json target description
typedef struct {
    // rest API target name
    char text[TARGET_NAME_LEN];
    // host name of the rest API target
    char hostname[HOSTNAME_LENGTH];
    // server details
    struct sockaddr_in serverAddr;
    // rest API port
    uint16_t port;
    // rest API resource identifier
    char resource[RESOURCE_LENGTH];
    // complete rest API HTTP request
    char request[REQUEST_LENGTH];
    // file descriptor of connection socket
    int socketFd;
    // number of values to build from json object
    int valueCount;
    // translated values (maybe scaled) array
    double *values;
    // json object key to values mapping array
    keysToValueMap_t *mapping;
} restTarget_t;

typedef struct {
    // (start) index of source register
    int regIndex;
    // number of registers
    int regCount;
    // index of destination field element
    int valueIndex;
    // index of scale factor to be used (-1 if no scaling necessary)
    int scaleFactorIndex;
} regsToValueMap_t;

// modbus register block description
typedef struct {
    // register type: 3 ... holding, 4 ... input
    int regType;
    // register number (already reduced by 1)
    int baseAddr;
    // number of registers to read
    int regCount;
    // register values array
    uint16_t *reg;
    // number of values to build from registers
    int valueCount;
    // translated values (maybe scaled) array
    double *values;
    // registers to values mapping array
    regsToValueMap_t *mapping;
} modbus_regs_block_t;

// modbus device description
typedef struct {
    // modbus/tcp or /rtu device id
    int deviceId;
    // device name
    char name[DEVICE_NAME_LEN];
    // device name
    char filename[FILENAME_LEN];
    // device name
    FILE *file;
    // number of blocks to read
    int blockCount;
    // register blocks array
    modbus_regs_block_t *block;
} device_t;

// modbus/tcp target description
typedef struct {
    // modbus/tcp target name
    char text[TARGET_NAME_LEN];
    // ip address of the modbus/tcp target
    char ip[IP_LENGTH];
    // modbus/tcp port
    uint16_t port;
    // modbus/tcp target object
    modbus_t *target;
    // number of devices to read
    int deviceCount;
    // devices array
    device_t *device;
} target_t;

#define VERBOSITY_INFO 3
#define VERBOSITY_WARN 2
#define VERBOSITY_ERROR 1

typedef struct {
    // error recovery mode
    modbus_error_recovery_mode errorRecoveryMode;
    // time offset of server (60 minutes on unifi-pi)
    long timeOffset;
    // number of targets to read
    int targetCount;
    // devices array
    target_t *target;
    // verbosity level
    int verbosityLevel;
} config_t;

// initialize the rest API targets
int initRestTargets(restTarget_t target[], int targetCount);

// read rest API targets
int readRestTargets(restTarget_t target[], int targetCount);

// read the json response from rest API
int readJsonResponse(restTarget_t *target);

// extract json objects denoted by key value mapping into values array
void mapJsonValues(restTarget_t *target, char jsonString[]);

// read inverter multiple times, if previous calls failed
int readTargetRetrying(target_t *target, int retryDelayMinutes, char timeNowText[], char colSep, bool doFlush);

// wait for a number of minutes, returns immediately if < 0
void waitForMinutes(int minutes);

// refresh current time and modify it by timeOffset (unifi pi: runs in GMT, so add 1 hour)
// returns time of day in minutes
int refreshTimeNow(struct tm *tm, long timeOffset, char text[]);

// fill time stamp string
void fillTimeNowText(char timeStamp[], struct tm tm);

// fill time stamp string for energy log
void fillTimeNowLogText(char timeStamp[], struct tm tm);

// calc minute of the day
int calcMinuteNow(struct tm tm);

// the signal handler for CTRL+C
void catchIntSignal(int sig_num);

// read invertr values
int readTarget(target_t *target, char timeStamp[], char colSep, bool doFlush);

// map register contents to values
int mapValues(modbus_regs_block_t *block);

// show register values
int showValues(modbus_regs_block_t *block, char colSep);

// format double value (Example: 123791,045)
char *formatDouble(char text[], double value);

// restore original inverter values
int closeTarget();

// reconnect to server
int reconnect(modbus_t *target, char timeStamp[]);

#endif //ENERGY_LOGGER_ENERGY_LOGGER_H