#include <Arduino.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "app_services.h"
#include "app_tasks.h"
#include "debugConsole.h"
#include "utils.h"
#include "app_monitor.h"

// Das Signal-Bit: 1 = Internet da, 0 = Funkstille
#define WIFI_STA_CONNECTED_BIT BIT0 // Verbunden mit Router
#define WIFI_AP_ACTIVE_BIT BIT1     // ESP ist selbst ein Access Point
#define SYSTEM_CONFIG_MODE_BIT BIT2 // Wir sind im Einstellungs-Modus

static TaskHandle_t hTaskClock = NULL;
static TaskHandle_t hTaskNetwork = NULL;
static TaskHandle_t hTaskTemperature = NULL;
static TaskHandle_t hTaskEnergy = NULL;
static TaskHandle_t hTaskPid = NULL;
static TaskHandle_t hTaskWeb = NULL;
static TaskHandle_t hTaskMaintenance = NULL;
static TaskHandle_t hTaskWatchdog = NULL;
static TaskHandle_t hTaskWifi = NULL;

EventGroupHandle_t wifi_event_group = NULL;

/* extern "C"
{
    void vTaskList(char *buffer);
} */
extern "C" uint32_t ulGetRunTimeCounterValue(void)
{
    return (uint32_t)(esp_timer_get_time()); // µs
}

extern "C" void vConfigureTimerForRunTimeStats(void)
{
    // nothing needed for ESP32
}

static void taskClock(void *pvParameters)
{
    int wdId = watchdogRegister("Clock", 10000);

    for (;;)
    {
        watchdogKick(wdId);
        serviceClock();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void taskNetwork(void *pvParameters)
{
    int wdId = watchdogRegister("Network", 10000);

    for (;;)
    {

        EventBits_t uxBits = xEventGroupWaitBits(wifi_event_group, WIFI_STA_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(100));

        if ((uxBits & WIFI_STA_CONNECTED_BIT) != 0)
        {
            watchdogKick(wdId);
            serviceNetworkSupervisor();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void taskTemperature(void *pvParameters)
{
    int wdId = watchdogRegister("Temperature", TASK_TEMPERATURE_INTERVAL);

    for (;;)
    {
        watchdogKick(wdId);
        serviceTemperature();
        vTaskDelay(pdMS_TO_TICKS(TASK_TEMPERATURE_INTERVAL));
    }
    
}

static void taskEnergy(void *pvParameters)
{

    int wdId = watchdogRegister("Energy", 10000);
    for (;;)
    {
        watchdogKick(wdId);
        xEventGroupWaitBits(wifi_event_group, WIFI_STA_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        serviceEnergy();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void taskPid(void *pvParameters)
{

    int wdId = watchdogRegister("Pid", 10000);
    for (;;)
    {
        watchdogKick(wdId);
        servicePid();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void taskWeb(void *pvParameters)
{
    int wdId = watchdogRegister("Web", 100000);
   

    for (;;)
    {

        xEventGroupWaitBits(wifi_event_group, WIFI_STA_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        bool heartbeatDue = false;
        bool activeStreaming = isStreamingAllowed(heartbeatDue);
        if (activeStreaming)
        {

            watchdogKick(wdId);
            serviceWeb();
           
        }
        else if (heartbeatDue)
        {
            // NACHT-MODUS (Heartbeat): Nur den allerneuesten Stand senden
            watchdogKick(wdId);
            serviceWeb();
        }
        else
        {
            LOG_DEBUG(TAG_APP_SERVICES, "Nachtmodus: Kein Streaming, kein Heartbeat fällig");
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void taskMaintenance(void *pvParameters)
{
    int wdId = watchdogRegister("Maintenance", 100000);
    for (;;)
    {
        watchdogKick(wdId);
        serviceMaintenance();
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void appTasks_init(bool apMode)
{
    wifi_event_group = xEventGroupCreate();

    if (wifi_event_group == NULL)
    {
        LOG_INFO(TAG_APP_TASKS, "app_task: Event Group konnte nicht erstellt werden! Zu wenig RAM?");
        while (1)
            ; // Stop hier, sonst Crash in den Tasks
    }
    if (apMode)
    {
        LOG_INFO(TAG_APP_TASKS, "Starting in AP mode, no WiFi connection will be attempted");
        xEventGroupSetBits(wifi_event_group, WIFI_AP_ACTIVE_BIT); // Alle Tasks laufen lassen, auch ohne WiFi
    }

    LOG_INFO(TAG_APP_TASKS, "Starting in normal mode, waiting for WiFi connection...");
    xEventGroupSetBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT); // Alle Tasks laufen lassen, auch ohne WiFi
}

void appTask_setupAPMode()
{
    // WiFi.mode(WIFI_AP);
    // WiFi.softAP("ESP32-Config-Portal", "12345678");
    xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
    xEventGroupClearBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT);
    xEventGroupSetBits(wifi_event_group, WIFI_AP_ACTIVE_BIT);

    LOG_INFO(TAG_APP_TASKS, "appTask_setupAPMode: AP Mode aktiv: 192.168.4.1");
}
void appTask_setupWifoMode()
{
    xEventGroupClearBits(wifi_event_group, WIFI_AP_ACTIVE_BIT);
    xEventGroupClearBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT);
    xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECTED_BIT); //

    LOG_INFO(TAG_APP_TASKS, "appTask_setupWifoMode: Normaler Modus, WiFi-Verbindung wird aufgebaut");
}

void appTask_setupSystemConfigMode()
{
    xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
    xEventGroupClearBits(wifi_event_group, WIFI_AP_ACTIVE_BIT);
    xEventGroupSetBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT); // Alle Tasks laufen lassen, auch ohne WiFi
    LOG_INFO(TAG_APP_TASKS, "appTask_setupSystemConfigMode: System-Config Mode aktiv, Warte auf Setup-Daten");
}

static void taskWiFi(void *pvParameters)
{
    // int wdId = watchdogRegister("WiFi", 10000);

    WifiCredentials *creds = (WifiCredentials *)pvParameters;
    LOG_INFO(TAG_APP_TASKS, "WiFi-Manager gestartet auf Core 0");

    while (1)
    {
        uint8_t status = WiFi.status();

        if (status != WL_CONNECTED)
        {
            // 1. Signal auf "Rot" (Bit löschen), damit andere Tasks wissen: Funkstille
            xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);

            LOG_INFO(TAG_APP_TASKS, "WiFi Verbindung verloren (Status: %d). Reconnect...", status);

            // Versuche Verbindung aufzubauen (falls nicht schon durch Auto-Reconnect aktiv)
            WiFi.begin(creds->ssid, creds->password);

            int retry = 0;
            // Max 15 Sekunden (30 * 500ms) warten
            while (WiFi.status() != WL_CONNECTED && retry < 30)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                retry++;
            }

            if (WiFi.status() == WL_CONNECTED)
            {
                LOG_INFO(TAG_APP_TASKS, "WiFi verbunden! IP: %s", WiFi.localIP().toString().c_str());
                // 2. Signal auf "Grün" (Bit setzen) - Andere Tasks dürfen wieder senden
                xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
            }
            else
            {
                LOG_ERROR(TAG_APP_TASKS, "WiFi Reconnect fehlgeschlagen. Neuer Versuch in 10s...");
                vTaskDelay(pdMS_TO_TICKS(10000));
                continue; // Sofort zum nächsten Schleifendurchlauf
            }
        }

        // Alle 5 Sekunden prüfen, wenn alles ok ist
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
/*
            MONITORING-TASK: Zeigt alle 2 Sekunden die Task-Liste und den freien Heap an. Hilfreich für Debugging und Performance-Überwachung.
*/

static void taskSimpleMonitor(void *pv)
{
    for (;;)
    {
        LOG_INFO("MON", "----- STACK MONITOR -----");

        /* for (int i = 0; i < getRegisteredTaskCount(); i++)
        {
            TaskEntry &entry = getTaskHandle(i);
            TaskHandle_t h = entry.handle;

            if (h != NULL)
            {
                if (entry.name == NULL || entry.name[0] == '\0')
                {
                    LOG_ERROR("MON", "Task %d hat keinen Namen!", i);
                    
                }
                else
                {
                    LOG_DEBUG("MON", "Checking task: %s", entry.name);
                    uint32_t stack = uxTaskGetStackHighWaterMark(h) * 4;

                    LOG_INFO("MON", "%s stack: %u bytes",
                             entry.name,
                             stack);

                    if (stack < 300)
                    {
                        LOG_ERROR("MON", "%s STACK CRITICAL!", entry.name);
                    }
                }
            }
            else
            {
                LOG_ERROR("MON", "%s NOT RUNNING", entry.name);
            }
        } */

        LOG_INFO("MON", "Free Heap: %u", esp_get_free_heap_size());

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void createAppTasks(WifiCredentials &credentials)
{
    // Initialisierung der Gruppe

    if (wifi_event_group == NULL)
    {
        wifi_event_group = xEventGroupCreate();
    }

    /// wdMutex = xSemaphoreCreateMutex();

    LOG_INFO(TAG_APP_TASKS, "main:: Creating App Tasks, but waiting 3 seconds for setup to be done");
    vTaskDelay(pdMS_TO_TICKS(3000));
    // WARTEN, bis setup() fertig ist
    LOG_INFO(TAG_APP_TASKS, "main:: Creating App Tasks, waiting for setup to be done");
    // xEventGroupWaitBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    /// CORE 0

    

    BaseType_t res = xTaskCreatePinnedToCore(taskNetwork, "taskNetwork", 4096, nullptr, 2, &hTaskNetwork, 0);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskNetwork!");
    }
    registerTask("Network", hTaskNetwork);

    res = xTaskCreatePinnedToCore(taskWeb, "taskWeb", 16384, nullptr, 1, &hTaskWeb, 0);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskWeb!");
    }
    registerTask("Web", hTaskWeb);
    res = xTaskCreatePinnedToCore(taskMaintenance, "taskMaintenance", 8192, nullptr, 1, &hTaskMaintenance, 0);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskMaintenance!");
    }
    registerTask("Maintenance", hTaskMaintenance);

    res = xTaskCreatePinnedToCore(taskWiFi, "taskWiFi", 8192, (void *)(&credentials), 2, &hTaskWifi, 0);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskWiFi!");
    }
    registerTask("WiFi", hTaskWifi);

    res = xTaskCreatePinnedToCore(taskWatchdog, "taskWatchdog", 4096, nullptr, 1, &hTaskWatchdog, 0);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskWatchdog!");
    }
    registerTask("Watchdog", hTaskWatchdog);

    // CORE 1

    res = xTaskCreatePinnedToCore(taskClock, "taskClock", 8192, nullptr, 1, &hTaskClock, 1);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskTemperature!");
    }

    res = xTaskCreatePinnedToCore(taskNetwork, "taskNetwork", 4096, nullptr, 2, &hTaskNetwork, 0);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskNetwork!");
    }
    registerTask("Clock", hTaskClock);
    res = xTaskCreatePinnedToCore(taskTemperature, "taskTemperature", 4096, nullptr, 2, &hTaskTemperature, 1);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskTemperature!");
    }
    registerTask("Temperature", hTaskTemperature);
    res = xTaskCreatePinnedToCore(taskEnergy, "taskEnergy", 8192, nullptr, 2, &hTaskEnergy, 1);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskEnergy!");
    }
    registerTask("Energy", hTaskEnergy);

    res = xTaskCreatePinnedToCore(taskPid, "taskPid", 16384, nullptr, 2, &hTaskPid, 1);
    if (res != pdPASS)
    {
        LOG_ERROR(TAG_APP_TASKS, "Failed to create taskPid!");
    }
    registerTask("PID", hTaskPid);

    xTaskCreatePinnedToCore(
        taskSimpleMonitor,
        "RuntimeMon",
        8192,
        nullptr,
        1,
        nullptr,
        0);
}