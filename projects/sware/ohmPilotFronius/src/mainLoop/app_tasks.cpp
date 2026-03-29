#include "app_tasks.h"
#include <Arduino.h>
#include "app_services.h"

static void taskClock(void *pvParameters)
{
    for (;;)
    {
        serviceClock();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void taskNetwork(void *pvParameters)
{
    for (;;)
    {
        serviceNetworkSupervisor();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void taskTemperature(void *pvParameters)
{
    for (;;)
    {
        serviceTemperature();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void taskEnergy(void *pvParameters)
{
    for (;;)
    {
        serviceEnergy();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void taskPid(void *pvParameters)
{
    for (;;)
    {
        servicePid();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void taskWeb(void *pvParameters)
{
    for (;;)
    {
        serviceWeb();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void taskMaintenance(void *pvParameters)
{
    for (;;)
    {
        serviceMaintenance();
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void createAppTasks()
{
    xTaskCreatePinnedToCore(taskClock, "taskClock", 4096, nullptr, 1, nullptr, 1);
    xTaskCreatePinnedToCore(taskNetwork, "taskNetwork", 4096, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(taskTemperature, "taskTemperature", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskEnergy, "taskEnergy", 8192, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskPid, "taskPid", 4096, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(taskWeb, "taskWeb", 8192, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(taskMaintenance, "taskMaintenance", 8192, nullptr, 1, nullptr, 0);
}