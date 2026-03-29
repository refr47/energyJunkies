#include <Arduino.h>
#include "esp_clk.h"
#include <SPI.h>
#include "esp32-hal-log.h"
#include "esp_heap_trace.h"
#include "esp_wifi.h"

#include "debugConsole.h"
#include "wlan.h"
#ifdef FRONIUS_IV
#include "modbusReader.h"
#include "froniusSolarAPI.h"
#elif HUAWEI_IV
#include "huawei.h"
#endif
#ifdef CARD_READER
#include "cardRW.h"
#endif
#include "utils.h"
#include "tft.h"
#include "eprom.h"
#include "pinManager.h"
#include "pin_config.h"
#include "www.h"
#include "temp.h"
#include "curTime.h"
#include "webSockets.h"
#include "weather.h"
#include "ledHandler.h"
#include "logReader.h"
#ifdef MQTT
#include "mqtt.h"
#endif

#ifdef INFLUX
#include "influx.h"
#endif
#ifdef AMIS_READER_DEV
#include "amisReader.h"
#endif
#include "energieManager.h"
#include "hotUpdate.h"
#include "shelly.h"
#include "system.h"

#include "app_sync.h"
#include "app_state.h"
#include "app_tasks.h"
// #include "app_tasksfroniusSolarAPI.h"

static RTC_DATA_ATTR int bootCount = 0;

WEBSOCK_DATA &getDataForWebSocket()
{
    return g_app.webSockData;
}

bool &setSetupChanged(bool didSetupChanged)
{
    g_app.webSockData.setupData.setupChanged = didSetupChanged;
    return g_app.webSockData.setupData.setupChanged;
}

void logging_init()
{
    DBGf("main::logging_init() - log level: %d", LOG_LEVEL_ESP);

    esp_log_level_set("*", ESP_LOG_VERBOSE);
    esp_log_level_set("ArduinoJson", ESP_LOG_ERROR);
    esp_log_level_set("AsyncTCP-esphome", ESP_LOG_ERROR);
    esp_log_level_set("ESPAsyncWebServer-esphome", ESP_LOG_ERROR);
    esp_log_level_set("modbus-esp8266", ESP_LOG_ERROR);
    esp_log_level_set("wifi", ESP_LOG_ERROR);

#if defined(LOG_LEVEL_ESP) && (LOG_LEVEL_ESP == 1)
    esp_log_level_set(TAG, ESP_LOG_ERROR);
#endif
#if defined(LOG_LEVEL_ESP) && (LOG_LEVEL_ESP == 2)
    esp_log_level_set(TAG, ESP_LOG_WARNING);
#endif
#if defined(LOG_LEVEL_ESP) && (LOG_LEVEL_ESP == 3)
    esp_log_level_set(TAG, ESP_LOG_INFO);
#endif
#if defined(LOG_LEVEL_ESP) && (LOG_LEVEL_ESP == 4)
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
#if defined(LOG_LEVEL_ESP) && (LOG_LEVEL_ESP == 5)
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);
#endif

    esp_log_set_vprintf(debug_LogOutput);

#ifdef CARD_READER
    esp_log_set_vprintf(cardRW_LogOutput);
    if (!cardRW_createLoggingFile())
    {
        ESP_LOGE(TAG, "Cannot create logging file on sd card");
    }
#endif
}

void setup()
{
    DBGbgn(115200);
    btStop();
    logging_init();

    appSyncInit();
    appStateInit();

    bootCount++;
    LOG_INFO("Boot number: %d", bootCount);

    ledHandler_init();
    tft_init();
    tft_printSetup();

    eprom_isInit();
    // eprom_test_write_Eprom("Milchbehaelter", "47754775");
    eprom_getSetup(g_app.webSockData.setupData);

    if (strcmp(g_app.webSockData.setupData.ssid, EMPTY_VALUE_IN_SETUP) == 0)
    {
        g_app.networkCredentialsInEEprom = false;
        g_app.webSockData.states.networkOK = false;
        www_init(g_app.webSockData.setupData, NULL, NULL, getDataForWebSocket, setSetupChanged);
    }

    if (g_app.networkCredentialsInEEprom)
    {
        if (!wifi_init(g_app.webSockData.setupData))
        {
            tft_clearScreen();
            wifi_scan_network();
            www_init(g_app.webSockData.setupData, NULL, NULL, getDataForWebSocket, setSetupChanged);
            g_app.webSockData.states.networkOK = false;
        }
        else
        {
            char *pBuf = g_app.globalStringBuffer;
            wifi_getLocalIP(&pBuf);
            tft_drawNetworkInfo(g_app.globalStringBuffer, g_app.webSockData.setupData.ssid);
            g_app.webSockData.states.flashOK = www_init(
                g_app.webSockData.setupData,
                pBuf,
                g_app.webSockData.setupData.ssid,
                getDataForWebSocket,
                setSetupChanged);
            g_app.webSockData.states.networkOK = true;
        }

        g_app.webSockData.states.timeServer = time_init();
        if (g_app.webSockData.states.timeServer)
        {
            g_app.secondsCounter = time_getOffset();
        }
        g_app.webSockData.states.modbusOK = false;
#ifdef FRONIUS_IV
        g_app.webSockData.states.modbusOK = mb_init(g_app.webSockData.setupData);
#endif
#ifdef MQTT
        g_app.webSockData.states.mqtt = mqtt_init();
#endif

#ifdef CARD_READER
        g_app.webSockData.states.cardWriterOK = cardRW_setup(false, false);
#endif

        g_app.webSockData.states.tempSensorOK = temp_init();

#ifdef FRONIUS_IV
        bool akkuAvailable = false;
        g_app.webSockData.setupData.externerSpeicher = false;
        g_app.webSockData.states.froniusAPI = false;

        if (soloar_init(g_app.webSockData, &akkuAvailable))
        {
            g_app.webSockData.setupData.externerSpeicher = akkuAvailable;
            if (solar_get_powerflow(g_app.webSockData))
            {
                g_app.webSockData.states.froniusAPI = true;
            }
        }
#endif

        g_app.pinManager.config(
            g_app.webSockData,
            RELAY_L1,
            RELAY_L2,
            PWM_FOR_PID);

#ifdef INFLUX
        g_app.webSockData.states.influx = influx_init(getDataForWebSocket);
#endif

#ifdef AMIS_READER_DEV
        g_app.webSockData.states.amisReader = amisReader_initRestTargets(g_app.webSockData);
#endif

#ifdef SHELLY
        if (g_app.webSockData.states.networkOK)
        {
            shelly_init(&g_app.webSockData.shellyObj[0]);
        }
#endif
    }

    logReader_init();
    createAppTasks();

    if (g_app.webSockData.states.networkOK)
    {

        HEAP_SIZE heapSize[2];

        heapSize[1].heapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        heapSize[1].heapSizeMax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        LOG_DEBUG("Free heap previous: %d largest block previous: %d,Free heap : %d largest block : %d", heapSize[0].heapSize, heapSize[0].heapSizeMax, heapSize[1].heapSize, heapSize[1].heapSizeMax);

        LOG_INFO(" -------- States ---------------");
        LOG_INFO("Network: %c", g_app.webSockData.states.networkOK == true ? 'y' : 'n');
        LOG_INFO("IP-Address: %s", g_app.webSockData.setupData.currentIP);
        LOG_INFO("Fronius: %c", g_app.webSockData.states.froniusAPI == true ? 'y' : 'n');
        LOG_INFO("AmisReader: %c", g_app.webSockData.states.amisReader == true ? 'y' : 'n');
        LOG_INFO("CardWrite: %c", g_app.webSockData.states.cardWriterOK == true ? 'y' : 'n');
        LOG_INFO("FlashFS: %c", g_app.webSockData.states.flashOK == true ? 'y' : 'n');
        LOG_INFO("Influx: %c", g_app.webSockData.states.influx == true ? 'y' : 'n');
        LOG_INFO("Modbus: %c", g_app.webSockData.states.modbusOK == true ? 'y' : 'n');
        LOG_INFO("MqTT: %c", g_app.webSockData.states.mqtt == true ? 'y' : 'n');
        LOG_INFO("TempSensor: %c", g_app.webSockData.states.tempSensorOK == true ? 'y' : 'n');
        LOG_INFO("TimeServer(NTP): %c", g_app.webSockData.states.timeServer == true ? 'y' : 'n');
        LOG_INFO("LogReader: y");
        LOG_INFO("HeapSizeDiff after Initializing: %d", heapSize[1].heapSize - heapSize[0].heapSize);
        tft_clearScreen();
        delay(5000);
    }
    LOG_INFO("Setup done - RTOS tasks started");
}

void loop()
{
    // Warte 5000 Millisekunden (5 Sekunden)
    vTaskDelay(pdMS_TO_TICKS(5000));
    // Alle 5 Sekunden auf Serial ausgeben:
    LOG_INFO("Free Heap: %u | Max Alloc: %u | Connects: %d | Free Stack: %d bytes",
             ESP.getFreeHeap(),
             ESP.getMaxAllocHeap(),
             WiFi.softAPgetStationNum(),
             uxTaskGetStackHighWaterMark(NULL)); // Falls im AP Modus
                                                 // Zeigt an, wie viele Bytes der Stack vom "Abgrund" (Canary) noch entfernt ist
}
