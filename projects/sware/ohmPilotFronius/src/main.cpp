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

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG_WLAN, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_TEMP, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_PID, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_MQTT, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_APP_SERVICES, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_WEB_SOCKETS, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_WEB, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_APP_TASKS, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_SHELLY, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_CARD, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_MODBUS, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_FRONIUS, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_AMIS, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_WEATHER, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_INFLUX, ESP_LOG_DEBUG);

    //  esp_log_set_vprintf(debug_LogOutput);

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
    vTaskDelay(500 / portTICK_PERIOD_MS);
    DBGbgn(115200);
    btStop();
    logging_init();

    appSyncInit();
    appStateInit();

    bootCount++;
    LOG_INFO(TAG_MAIN, "Boot number: %d", bootCount);

    ledHandler_init();
    tft_init();
    tft_printSetup();

    //eprom_isInit();
    // eprom_test_write_Eprom("FRITZ!Box 7530 YK", "reitinger");
    //eprom_test_write_Eprom("Milchbehaelter", "47754775");
    if (!eprom_getSetup(g_app.webSockData.setupData)) {
        eprom_test_write_Eprom("Milchbehaelter", "47754775");
    }

    if (strcmp(g_app.webSockData.setupData.ssid, EMPTY_VALUE_IN_SETUP) == 0)
    {
        // AP mode
        g_app.networkCredentialsInEEprom = false;
        LOG_DEBUG(TAG_MAIN, "main:: No network credentials in eeprom, starting in AP mode");
        g_app.networkCredentialsInEEprom = false;
        g_app.webSockData.states.networkOK = false;
        g_app.networkCredentialsInEEprom = false;
        appTasks_init(true);
        www_init(g_app.webSockData.setupData, NULL, NULL, getDataForWebSocket, setSetupChanged);
    }
    else
    {
        LOG_DEBUG(TAG_MAIN, "main:: Network credentials in eeprom, trying to connect to wifi");
        g_app.networkCredentialsInEEprom = true;
        appTasks_init(false); // ap-mode false, because wifi found
    }

    if (g_app.networkCredentialsInEEprom)
    {
        // network, password in eeprom, try to connect to wifi
        LOG_DEBUG(TAG_MAIN, "main:: wifi init");
        // appTask_setupSystemConfigMode(); // Set the system to configuration mode
        if (!wifi_init(g_app.webSockData.setupData))
        {
            tft_clearScreen();
            // wifi_scan_network();
            appTasks_init(true); // AP mode, because wifi connection failed
            LOG_DEBUG(TAG_MAIN, "main:: www_init ");
            www_init(g_app.webSockData.setupData, NULL, NULL, getDataForWebSocket, setSetupChanged);
            g_app.webSockData.states.networkOK = false;
        }
        else
        {
            char *pBuf = g_app.globalStringBuffer;

            LOG_DEBUG(TAG_MAIN, "main:: set app task to normal mode, because wifi connected");
            wifi_getLocalIP(&pBuf);
            appTask_setupWifoMode();
            LOG_DEBUG(TAG_MAIN, "main:: set app task to normal mode done");
            tft_drawNetworkInfo(g_app.globalStringBuffer, g_app.webSockData.setupData.ssid);
            LOG_INFO(TAG_MAIN, "main:: www_init for network %s mode with IP: %s", g_app.webSockData.setupData.ssid, g_app.globalStringBuffer);
            g_app.webSockData.states.flashOK = www_init(
                g_app.webSockData.setupData,
                pBuf,
                g_app.webSockData.setupData.ssid,
                getDataForWebSocket,
                setSetupChanged);
            g_app.webSockData.states.networkOK = true;
        }
        LOG_DEBUG(TAG_MAIN, "main:: time_init");
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

        g_app.pinManager.testPins(RELAY_L1, RELAY_L2, PWM_FOR_PID);

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
    WifiCredentials credentials;
    strncpy(credentials.ssid, g_app.webSockData.setupData.ssid, LEN_WLAN);
    strncpy(credentials.password, g_app.webSockData.setupData.passwd, LEN_WLAN);
    credentials.apMode = g_app.networkCredentialsInEEprom;
    logReader_init();

    if (g_app.webSockData.states.networkOK)
    {

        HEAP_SIZE heapSize[2];

        heapSize[1].heapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        heapSize[1].heapSizeMax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        LOG_DEBUG(TAG_MAIN, "Free heap previous: %d largest block previous: %d,Free heap : %d largest block : %d", heapSize[0].heapSize, heapSize[0].heapSizeMax, heapSize[1].heapSize, heapSize[1].heapSizeMax);

        LOG_INFO(TAG_MAIN, " -------- States ---------------");
        LOG_INFO(TAG_MAIN, "Network: %c", g_app.webSockData.states.networkOK == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "IP-Address: %s", g_app.webSockData.setupData.currentIP);
        LOG_INFO(TAG_MAIN, "Fronius: %c", g_app.webSockData.states.froniusAPI == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "AmisReader: %c", g_app.webSockData.states.amisReader == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "CardWrite: %c", g_app.webSockData.states.cardWriterOK == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "FlashFS: %c", g_app.webSockData.states.flashOK == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "Influx: %c", g_app.webSockData.states.influx == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "Modbus: %c", g_app.webSockData.states.modbusOK == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "MqTT: %c", g_app.webSockData.states.mqtt == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "TempSensor: %c", g_app.webSockData.states.tempSensorOK == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "TimeServer(NTP): %c", g_app.webSockData.states.timeServer == true ? 'y' : 'n');
        LOG_INFO(TAG_MAIN, "ForceHeating: %d", g_app.webSockData.setupData.forceHeating);
        LOG_INFO(TAG_MAIN, "LogReader: y");
        LOG_INFO(TAG_MAIN, "HeapSizeDiff after Initializing: %d", heapSize[1].heapSize - heapSize[0].heapSize);
        tft_clearScreen();
        delay(5000);
    }

    
    createAppTasks(credentials);

    LOG_INFO(TAG_MAIN, "Setup done - RTOS tasks started");
    // void appTask_setupSystemConfigMode();
}

void loop()
{
    // Warte 5000 Millisekunden (5 Sekunden)
    vTaskDelay(pdMS_TO_TICKS(10000));
    // Alle 5 Sekunden auf Serial ausgeben:
    LOG_INFO(TAG_MAIN, "Free Heap: %u | Max Alloc: %u | Connects: %d | Free Stack: %d bytes",
             ESP.getFreeHeap(),
             ESP.getMaxAllocHeap(),
             WiFi.softAPgetStationNum(),
             uxTaskGetStackHighWaterMark(NULL)); // Falls im AP Modus
                                                 // Zeigt an, wie viele Bytes der Stack vom "Abgrund" (Canary) noch entfernt ist
}
