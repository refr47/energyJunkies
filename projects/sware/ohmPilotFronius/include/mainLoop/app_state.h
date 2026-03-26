#pragma once

#include <Arduino.h>
#include "app_sync.h"

#include "pin_config.h"
#include "pidManager.h"
#include "webSockets.h"
#include "temp.h"
#include "system.h"

#define GLOBAL_STRING_BUFFER_LEN 150
#define FORMAT_CHAR_BUFFER_LEN 50
#define SECONDS_PER_DAY 86400
#define NUM_RECORDS 100

typedef struct _ALARM_TEMPERATURE
{
    bool alarmTemp;
    time_t overFlowHappenedAt;
} ALARM_TEMPERATURE;

typedef struct _ALARM_MODBUS
{
    bool avaiilable;
    time_t notAvailableAt;
} _ALARM_MODBUS;

typedef struct _ALARM
{
    ALARM_TEMPERATURE alarmTemp;
    _ALARM_MODBUS alarmModbus;
} ALARM_CONTAINER;

typedef struct _HEAP_SIZE
{
    unsigned int heapSize;
    unsigned int heapSizeMax;
} HEAP_SIZE;

typedef struct _APP_RUNTIME
{
    char globalStringBuffer[GLOBAL_STRING_BUFFER_LEN];
    char formatBuffer[FORMAT_CHAR_BUFFER_LEN];

    bool networkCredentialsInEEprom;

    WEBSOCK_DATA webSockData;
    ALARM_CONTAINER alarmContainer;
    PinManager pidPinManager;
    HEAP_SIZE heapSize[2];
    unsigned long secondsCounter;
} APP_RUNTIME;

extern APP_RUNTIME g_app;

bool appStateInit();
void appLock();
void appUnlock();