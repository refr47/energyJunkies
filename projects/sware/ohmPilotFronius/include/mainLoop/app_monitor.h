#pragma once

#include "defines.h"

#define MAX_TASKS 9


#define T_NETWORK 0
#define T_WEB 1
#define T_TEMPERATURE 2
#define T_ENERGY 3
#define T_PID 4
#define T_MAINTENANCE 5
#define T_WIFI 6
#define T_CLOCK 7
#define T_WATCHDOG 8

struct TaskWatchdogEntry
{
    const char *name;
    uint32_t lastKick;
    uint32_t timeoutMs;
};

/* typedef struct _TaskInfo
{
    const char *name;
    TaskHandle_t *handle;
} TaskInfo; */
typedef struct _TaskEntry
{
    const char *name;
    TaskHandle_t handle;
} TaskEntry;

int watchdogRegister(const char *name, uint32_t timeoutMs);
void watchdogKick(int id);
void taskWatchdog(void *pv);

void registerTask(const char *name, TaskHandle_t handle);
TaskEntry &getTaskHandle(unsigned index);
int getRegisteredTaskCount();
