#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_monitor.h"

static TaskWatchdogEntry wd[MAX_TASKS];
static int wdCount = 0;


static TaskEntry taskHandler[MAX_TASKS];
static int taskCount = 0;   

/* static TaskInfo taskList[] = {
    {"Temp", &hTaskTemperature},
    {"Network", &hTaskNetwork},
    {"PID", &hTaskPid},
    {"Web", &hTaskWeb},
    {"Energy", &hTaskEnergy},
    {"WiFi", &hTaskWifi},
    {"Clock", &hTaskClock},
    {"Maintenance", &hTaskMaintenance},
    {"Watchdog", &hTaskWatchdog},
}; */

//const int taskCount = sizeof(taskList) / sizeof(taskList[0]);


int watchdogRegister(const char *name, uint32_t timeoutMs)
{
    if (wdCount >= MAX_TASKS)
        return -1;

    wd[wdCount] = {name, millis(), timeoutMs};
    return wdCount++;
}

void watchdogKick(int id)
{
    if (id < 0 || id >= wdCount)
        return;

    wd[id].lastKick = millis();
}

 void taskWatchdog(void *pv)
{
    for (;;)
    {
        uint32_t now = millis();

         for (int i = 0; i < wdCount; i++)
        {
            if (now - wd[i].lastKick > wd[i].timeoutMs)
            {
                LOG_ERROR("WATCHDOG",
                          "Task %s TIMEOUT! Last kick: %u ms ago",
                          wd[i].name,
                          now - wd[i].lastKick);
            }
        } 

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void registerTask(const char *name, TaskHandle_t handle)
{
    if (taskCount < MAX_TASKS)
    {
        taskHandler[taskCount].name = name;
        taskHandler[taskCount++].handle = handle;
        taskCount++;
        
    }
}

TaskEntry &getTaskHandle(unsigned index)
{
    if (index < MAX_TASKS)
    {
        return taskHandler[index]  ;
    }
    static TaskEntry empty = {"", nullptr};
    return empty;
}

int getRegisteredTaskCount()
{
    return taskCount;
}
