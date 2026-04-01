#include <Arduino.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "app_services.h"
#include "app_tasks.h"
#include "debugConsole.h"

// Das Signal-Bit: 1 = Internet da, 0 = Funkstille
#define WIFI_STA_CONNECTED_BIT BIT0 // Verbunden mit Router
#define WIFI_AP_ACTIVE_BIT BIT1     // ESP ist selbst ein Access Point
#define SYSTEM_CONFIG_MODE_BIT BIT2 // Wir sind im Einstellungs-Modus

EventGroupHandle_t wifi_event_group = NULL;

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

        xEventGroupWaitBits(wifi_event_group, WIFI_STA_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
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
        xEventGroupWaitBits(wifi_event_group, WIFI_STA_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
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
        xEventGroupWaitBits(wifi_event_group, WIFI_STA_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
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

void appTasks_init(bool apMode)
{
    wifi_event_group = xEventGroupCreate();

    if (wifi_event_group == NULL)
    {
        LOG_INFO(TAG_APP_TASKS,"app_task: Event Group konnte nicht erstellt werden! Zu wenig RAM?");
        while (1)
            ; // Stop hier, sonst Crash in den Tasks
    }
    if (apMode)
    {
        LOG_INFO(TAG_APP_TASKS,"Starting in AP mode, no WiFi connection will be attempted");
        xEventGroupSetBits(wifi_event_group, WIFI_AP_ACTIVE_BIT); // Alle Tasks laufen lassen, auch ohne WiFi
    }

    LOG_INFO(TAG_APP_TASKS,"Starting in normal mode, waiting for WiFi connection...");
    xEventGroupSetBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT); // Alle Tasks laufen lassen, auch ohne WiFi
}


void appTask_setupAPMode()
{
    // WiFi.mode(WIFI_AP);
    // WiFi.softAP("ESP32-Config-Portal", "12345678");
    xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
    xEventGroupClearBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT);
    xEventGroupSetBits(wifi_event_group, WIFI_AP_ACTIVE_BIT);

    LOG_INFO(TAG_APP_TASKS,"appTask_setupAPMode: AP Mode aktiv: 192.168.4.1");
}
void appTask_setupWifoMode()
{
    xEventGroupClearBits(wifi_event_group, WIFI_AP_ACTIVE_BIT);
    xEventGroupClearBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT);
    xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECTED_BIT); //

    LOG_INFO(TAG_APP_TASKS,"appTask_setupWifoMode: Normaler Modus, WiFi-Verbindung wird aufgebaut");
}

void appTask_setupSystemConfigMode()
{
    xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
    xEventGroupClearBits(wifi_event_group, WIFI_AP_ACTIVE_BIT);
    xEventGroupSetBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT); // Alle Tasks laufen lassen, auch ohne WiFi
    LOG_INFO(TAG_APP_TASKS,"appTask_setupSystemConfigMode: System-Config Mode aktiv, Warte auf Setup-Daten");
}

static void taskWiFi(void *pvParameters)
{
    LOG_INFO(TAG_APP_TASKS,"WiFi-Manager gestartet auf Core 0");

    WifiCredentials *creds = (WifiCredentials *)pvParameters;

    EventBits_t bits = xEventGroupGetBits(wifi_event_group);

    while (1)
    {
        if (bits & WIFI_STA_CONNECTED_BIT)
        {

            if (WiFi.status() != WL_CONNECTED)
            {
                // Signal auf "Rot" setzen - Alle anderen Tasks pausieren jetzt
                xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);

                LOG_DEBUG(TAG_APP_TASKS,"appTask::taskWifi - WiFi verloren oder nicht initialisiert. Verbinde...");
                WiFi.begin(creds->ssid, creds->password);

                int retry = 0;
                // Max 15 Sekunden probieren
                while (WiFi.status() != WL_CONNECTED && retry < 30)
                {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    retry++;
                }

                if (WiFi.status() == WL_CONNECTED)
                {
                    LOG_INFO(TAG_APP_TASKS,"appTask::taskWifi - WiFi wieder da! IP: %s", WiFi.localIP().toString().c_str());
                    // Alle anderen Tasks laufen weiter
                    xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
                }
            }
        }
        else
        {
            LOG_DEBUG(TAG_APP_TASKS,"appTask::taskWifi - Warte auf WiFi-Verbindung...");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        bits = xEventGroupGetBits(wifi_event_group);
        // Alle 5 Sekunden den Status prüfen (schont die CPU)
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void createAppTasks(WifiCredentials &credentials)
{
    // Initialisierung der Gruppe

    if (wifi_event_group == NULL)
    {
        wifi_event_group = xEventGroupCreate();
    }
    LOG_INFO(TAG_APP_TASKS,"main:: Creating App Tasks, but waiting 10 seconds for setup to be done");
    vTaskDelay(pdMS_TO_TICKS(10000));
    // WARTEN, bis setup() fertig ist
    LOG_INFO(TAG_APP_TASKS,"main:: Creating App Tasks, waiting for setup to be done");
    // xEventGroupWaitBits(wifi_event_group, SYSTEM_CONFIG_MODE_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    xTaskCreatePinnedToCore(taskClock, "taskClock", 4096, nullptr, 1, nullptr, 1);
    // xTaskCreatePinnedToCore(taskNetwork, "taskNetwork", 4096, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(taskTemperature, "taskTemperature", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskEnergy, "taskEnergy", 8192, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskPid, "taskPid", 4096, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(taskWeb, "taskWeb", 8192, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(taskMaintenance, "taskMaintenance", 8192, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(taskWiFi, "taskWiFi", 6144, (void *)(&credentials), 10, NULL, 0);
}