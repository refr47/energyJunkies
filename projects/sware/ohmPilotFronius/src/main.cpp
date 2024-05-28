#include <Arduino.h>
#include "esp_clk.h"
#include <SPI.h>
#include <esp_log.h>
#include "esp_heap_trace.h"

#include "debugConsole.h"
#include "wlan.h"
#ifdef FRONIUS_IV
#include "modbusReader.h"
#elif HUAWEI_IV
#include "huawei.h"
#endif
#ifdef CARD_READER
#include "cardRW.h"
#endif
#include "utils.h"
#include "tft.h"
// #include "graphicTest.h"
#include "eprom.h"
#include "pidManager.h" // pid controller +
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
#ifdef FRONIUS_IV
#include "froniusSolarAPI.h"
#endif
#ifdef INFLUX
#include "influx.h"
#endif
#ifdef AMIS_READER_DEV
#include "amisReader.h"
#endif
#include "energieManager.h"
#include "hotUpdate.h"
/*
Input only pins
GPIOs 34 to 39 are GPIs – input only pins. These pins don’t have internal pull-up or pull-down resistors. They can’t be used as outputs, so use these pins only as inputs:

https://github.com/Xinyuan-LilyGO/T-Display-S3
*/

/* ****************************************************************************

  DEFInES

  ****************************************************************************
 */

#define GLOBAL_STRING_BUFFER_LEN 150
#define INTERNAL_BUTTON_1_GPIO 0
#define INTERNAL_BUTTON_2_GPIO 14

#define TEMPERATURE_INTERVAL 5000UL             // 5 secs
#define TEMPERATURE_OVERHEATED_WAIT_IN_SECS 300 // 5 minute wait after temp of buffer store has climbed over upper limit
#define MODBUS_INTERVALL 2000UL

#define PID_CONTROLLER_INTERVALL 1000 // 0.1 secs default sample time
#define LOGGING_FLUSH_INTERVALL 60000
#define MODBUS_AKKU_INTERVALL 10000
#define CLOCK_INTERVALL 1000           // secs
#define WEBSOCK_NOTIFY_INTERVALL 10000 // 5 secs
#define SHOW_IP_ADDR_INTERVALL 5000
#define CHECK_HEAP_SIZE_INTERVALL 60000 //
#define AMIS_READER_INTERVALL 10000
#define RECONNET_INTERVALL 5000 // 5 secs
#define SETUP_CHECK_INTERVALL 10000
#define FORMAT_CHAR_BUFFER_LEN 50 // @see loop

#define MAX_RECONNECTING_NET 5
#define DELAY_RECONNECT_NET 10000 // wait 10 secs for next connection

#define NUM_RECORDS 100
// static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
static RTC_DATA_ATTR int bootCount = 0;
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

typedef struct _MAX_RECONNECTING_
{

    unsigned int modbusCounter;
    unsigned int maxModbusCounter;

} MAX_RECONNECTING;

typedef struct _HEAP_SIZE
{
    unsigned int heapSize;
    unsigned int heapSizeMax;
} HEAP_SIZE;

typedef struct _TIME_SLICE
{
    unsigned long previousMillTemp;
    unsigned long previousMillModbus;
    unsigned long previousMillFlush;
    unsigned long previousMillisClock;
    unsigned long previousMillisWebSocks;
    unsigned long previousMillisShowIp;
    unsigned long previousMillisController;
    unsigned long previousMillisHeapCheck;
    unsigned long previousMillisAkku;
    unsigned long previousMillisAmis;
    unsigned long previousMillisReconnect;
    unsigned long previousMillisSetup;
    unsigned long currentMillis;

    MAX_RECONNECTING maxReconnecting;

} TIME_SLICE;

/* ****************************************************************************
  GLOBAL VARS https://randomnerdtutorials.com/esp32-websocket-server-sensor/
  ****************************************************************************
*/

char globalStringBuffer[GLOBAL_STRING_BUFFER_LEN];
static bool networkCredentialsInEEprom = true;

static TIME_SLICE timeSlice;

// static LIFE_DATA lifeData;
static ALARM_CONTAINER alarmContainer;
static PinManager pidPinManager;
static WEBSOCK_DATA webSockData;
static HEAP_SIZE heapSize[2];

// static double availablePowerFromWRInWatt = 0.0;

/* **************************************************************************
        ProtoTypes
*/

WEBSOCK_DATA &
getDataForWebSocket();
bool &setSetupChanged(bool didSetupChanged);

// int sdCardLogOutput(const char *format, va_list args); // LOG-System

// https://community.platformio.org/t/redirect-esp32-log-messages-to-sd-card/33734/5
void logging_init()
{

    LOG_INFO("Setting log levels and callback");
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);
    esp_log_set_vprintf(debug_LogOutput);

#ifdef CARD_READER
    esp_log_set_vprintf(cardRW_LogOutput);

    if (!cardRW_createLoggingFile())
    {
        ESP_LOGE(TAG, "Cannot create logging file on sd card");
    }
#endif
}

void test_cardReader()
{
#ifdef CARD_READER
    cardRW_listDir("/", 3);
#endif
}

void setup()
{

    DBGbgn(115200);
    /*   while (!Serial)
          ; */
    btStop(); // stop bluetoothd
    logging_init();
    LOG_INFO("Energie-Junkies -- Harvester ---");
    memset(&webSockData, 0, sizeof(WEBSOCK_DATA));
    memset(&timeSlice, 0, sizeof(TIME_SLICE));
    timeSlice.maxReconnecting.maxModbusCounter = 60000; // 1 min hat 60 secs
    bootCount++;
    LOG_DEBUG("Boot number: %d", bootCount);
    ledHandler_init();
    tft_init();
    tft_printSetup();
    heapSize[0].heapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    heapSize[0].heapSizeMax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    LOG_DEBUG("Free heap: %d largest block: %d", heapSize[0].heapSize, heapSize[0].heapSizeMax);

    /*  int currentState = digitalRead(Iheap_caps_get_largest_free_block()NTERNAL_BUTTON_2_GPIO);
     LOG_INFO("Interal button: %d", currentState); */

    /* uint32_t cpu_freq = esp_clk_cpu_freq();
    DBG(" CPU freq: ");
    DBGln(cpu_freq);
    uint32_t PRESCALE = 240; // for 240MHZ */

    // eprom_test_write_Eprom("Milchbehaelter", "47754775");
    //      eprom_clearLifeData();
    eprom_isInit();

    // ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
    // eprom_test_write_Eprom("Milchbehaelter", "47754775");
    eprom_getSetup(webSockData.setupData);
    // eprom_getLifeData(lifeData);

    // printEprom(webSockData.setupData);

    LOG_DEBUG("Free heap: %d largest block: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // EMPTY_VALUE_IN_SETUP= "--"
    if (strcmp(webSockData.setupData.ssid, EMPTY_VALUE_IN_SETUP) == 0)
    {
        networkCredentialsInEEprom = false;
        webSockData.states.networkOK = false;
        www_init(webSockData.setupData, NULL, NULL, getDataForWebSocket, setSetupChanged); // act as access point
    }

    if (networkCredentialsInEEprom)
    {

        /*       char buff[130];
              memset(buff, 0, strlen(buff)); */

        if (!wifi_init(webSockData.setupData))
        {
            LOG_WARNING("Cannot connect - show available networks: ");
            // tft_drawNetworkInfo(NULL, webSockData.setupData.ssid);
            tft_clearScreen();
            wifi_scan_network();
            www_init(webSockData.setupData, NULL, NULL, getDataForWebSocket, setSetupChanged); // act as access point
            webSockData.states.networkOK = false;
            // tft_printInfo("No valid network!");
            return;
        }
        else
        {

            char *pBuf = globalStringBuffer;
            wifi_getLocalIP(&pBuf);
            LOG_INFO("Connected with ip: %s", globalStringBuffer);

            tft_drawNetworkInfo(globalStringBuffer, webSockData.setupData.ssid);
            webSockData.states.flashOK = www_init(webSockData.setupData, pBuf, webSockData.setupData.ssid, getDataForWebSocket, setSetupChanged); // do not act as apoint
            webSockData.states.networkOK = true;
        }

        LOG_DEBUG("Free heap: %d largest block: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

        tft_printKeyValue("Init Time", "OK", TFT_GREEN);
        time_init(); // init time
        time_currentTimeStamp();

        LOG_DEBUG("Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
        LOG_INFO("Setup Modbus ...");
        webSockData.states.modbusOK = false;
        if (!mb_init(webSockData.setupData))
        {
            LOG_ERROR("Cannot initialize modbus ....");
            tft_printKeyValue("Init mdobus", "Error", TFT_RED);

            ledHandler_showModbusError(true);
        }
        else
        {
            webSockData.states.modbusOK = true;
            tft_printKeyValue("Init Modbus", "ok", TFT_GREEN);
        }

        LOG_DEBUG("Free heap: %d largest block: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

        // memset(&webSockData.mbContainer, 0, sizeof(MB_CONTAINER));

#ifdef MQTT
        if (!mqtt_init())
        {
            LOG_ERROR("Mqtt-Server -- cannot connect (!)");
        }
        else
            LOG_INFO("Mqtt-Server:: connected successfully ...");
#endif
        webSockData.states.cardWriterOK = false;
#ifdef CARD_READER
        if (cardRW_setup(false, false))
        {
            webSockData.states.cardWriterOK = true;
            tft_printKeyValue("Init CardReader", "OK", TFT_GREEN);

            logging_init();
            // test_cardReader();
        }
        else
        {
            LOG_ERROR("Logging to file cannot be initiated ...");
            tft_printKeyValue("Init CardReader", "Error", TFT_RED);

            ledHandler_showCardReaderError(true);
        }
#endif
        /* ESP_ERROR_CHECK(heap_trace_stop());
        heap_trace_dump(); */
        LOG_DEBUG("Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
        webSockData.states.tempSensorOK = false;
        if (temp_init())
        {

            tft_printKeyValue("Init Sensors", "OK", TFT_GREEN);
            webSockData.states.tempSensorOK = true;
        }
        else
        {

            tft_printKeyValue("Init Sensors", "Error", TFT_RED);
        }
#ifdef WEATHER_API
        wheater_getForecast();
        wheater_print();
#endif
        if (webSockData.states.modbusOK)
        {
            if (!mb_readAll(webSockData.setupData, webSockData.mbContainer))
            {
                LOG_ERROR("Init::readAllModbusValues did not succeed");
                tft_printKeyValue("Modbus Read", "Error", TFT_RED);
            }
        }

        webSockData.states.froniusAPI = false;

#ifdef FRONIUS_IV
        bool akkuAvailable = false;

        /* const char *cp = webSockData.setupData.inverter;
         if (cp == NULL)
             LOG_INFO("cp is null");
         else
             LOG_INFO("inverter is: %s", cp);
             */
        LOG_INFO("WR (FRonius detect solar api): %s ", webSockData.setupData.inverter);
        webSockData.setupData.externerSpeicher = false;
        webSockData.states.froniusAPI = false;
        if (soloar_init(webSockData, &akkuAvailable))
        {
            webSockData.setupData.externerSpeicher = akkuAvailable;
            LOG_INFO("main - akku: %d", webSockData.setupData.externerSpeicher);
            memset(&webSockData.fronius_SOLAR_POWERFLOW, 0, sizeof(FRONIUS_SOLAR_POWERFLOW));
            if (solar_get_powerflow(webSockData))
            {
                webSockData.states.froniusAPI = true;
                LOG_DEBUG("main1 - akku: %d", webSockData.setupData.externerSpeicher);
                tft_printKeyValue("Fronius Solar API", "Yes", TFT_GREEN);
                if (webSockData.setupData.externerSpeicher == true)
                    tft_printKeyValue("AKKU", "Yes", TFT_GREEN);
                else
                    tft_printKeyValue("AKKU", "No", TFT_GREEN);
                LOG_DEBUG("Fronius solar API Support: YES, AKKU: %s", webSockData.setupData.externerSpeicher == true ? "Yes" : "NO");
            }
            else
            {
                LOG_ERROR("setup::solar_init - Cannot access Fronius REST  SOLAR API form converter!");
                tft_printKeyValue(" Fronius Solar API", "ERROR", TFT_RED);
            }
        }

#endif

        LOG_INFO("Setup PID-Controller");
#ifdef MQTT
        mqtt_publish_pidParams(webSockData.setupData.pid_p, webSockData.setupData.pid_i, webSockData.setupData.pid_d);
#endif
        // LOG_INFO("Mqtt - PID params:  p: %.2lf  i: %.2lf    d: %.2lf", webSockData.setupData.pid_p, webSockData.setupData.pid_i, webSockData.setupData.pid_d);
        tft_printKeyValue("Init PID-Manager", "ok", TFT_GREEN);
        pidPinManager.config(webSockData.setupData, RELAY_L1, RELAY_L2, PWM_FOR_PID);
#ifdef INFLUX
        webSockData.states.influx = false;
        if (influx_init(getDataForWebSocket))
        {
            webSockData.states.influx = true;
        }
#endif
        webSockData.states.amisReader = false;

#ifdef AMIS_READER_DEV
        if (webSockData.states.froniusAPI == false && webSockData.states.modbusOK == false)
        {
            if (amisReader_initRestTargets(webSockData))
            {
                webSockData.states.amisReader = true;

                if (amisReader_readRestTarget(webSockData))
                {
                    webSockData.states.amisReader = true;
                    tft_printKeyValue("Init AMIS-Reader", "ok", TFT_GREEN);
                    LOG_INFO("AMIS-Reader:: connected successfully ...");
                }
                else
                {

                    tft_printKeyValue("Init AMIS-Reader", "Error", TFT_RED);
                    LOG_ERROR("AMIS-Reader:: connection failed ...");
                }
            }
            else
            {
                tft_printKeyValue("NO AMIS-Reader", "Error", TFT_RED);
                LOG_ERROR("amisReader not found");
            }
        }

#endif
    }
    logReader_init();

    if (webSockData.states.networkOK)
    {

        heapSize[1].heapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        heapSize[1].heapSizeMax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        LOG_DEBUG("Free heap previous: %d largest block previous: %d,Free heap : %d largest block : %d", heapSize[0].heapSize, heapSize[0].heapSizeMax, heapSize[1].heapSize, heapSize[1].heapSizeMax);
        LOG_INFO(" -------- States ---------------");
        LOG_INFO("Network: %c", webSockData.states.networkOK == true ? 'y' : 'n');
        LOG_INFO("IP-Address: %s", webSockData.setupData.currentIP);
        LOG_INFO("Fronius: %c", webSockData.states.froniusAPI == true ? 'y' : 'n');
        LOG_INFO("AmisReader: %c", webSockData.states.amisReader == true ? 'y' : 'n');
        LOG_INFO("CardWrite: %c", webSockData.states.cardWriterOK == true ? 'y' : 'n');
        LOG_INFO("FlashFS: %c", webSockData.states.flashOK == true ? 'y' : 'n');
        LOG_INFO("Influx: %c", webSockData.states.influx == true ? 'y' : 'n');
        LOG_INFO("Modbus: %c", webSockData.states.modbusOK == true ? 'y' : 'n');
        LOG_INFO("MqTT: %c", webSockData.states.mqtt == true ? 'y' : 'n');
        LOG_INFO("TempSensor: %c", webSockData.states.tempSensorOK == true ? 'y' : 'n');
        LOG_INFO("LogReader: y");
        LOG_INFO("HeapSizeDiff after Initializing: %d", heapSize[1].heapSize - heapSize[0].heapSize);
        tft_clearScreen();
        delay(5000);
    }
    LOG_INFO("Setup done - all components are working...");

    // eM_printWakeUpReason();
} // init

static char formatBuffer[FORMAT_CHAR_BUFFER_LEN];

void loop()
{
    /*   LOG_INFO("Wait for 20 secs in loop");
      delay(20000);
      LOG_INFO("in loop - after waitinger for 20 secs"); */
    heapSize[0].heapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    heapSize[0].heapSizeMax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    timeSlice.currentMillis = millis();

    if (!webSockData.states.networkOK)
    {

        delay(10000);
        WiFi.disconnect();
        if (wifi_isStillConnected(webSockData.setupData))
        {
            LOG_INFO("Network reconnected - tcp ip: %s", webSockData.setupData.currentIP);
            webSockData.states.networkOK = true;
        }
        else
        {
            pidPinManager.reset(); // alles aus
            LOG_ERROR("Network does not work - no further task are available, tcp ip: %s", webSockData.setupData.currentIP);
        }

        // timeSlice.currentMillis = millis();
    }
    /* ***********************                   CLOCK           ************************/
    if (timeSlice.currentMillis - timeSlice.previousMillisClock > CLOCK_INTERVALL)
    {
        ledHandler_blink();
        if (getCurrentTime(formatBuffer, FORMAT_CHAR_BUFFER_LEN))
        {

            tft_updateTime(formatBuffer);
        }
        else
        {
            tft_updateTime("00:00:00");
        }
#ifdef MQTT
        mqtt_loop();
#endif
        timeSlice.previousMillisClock = timeSlice.currentMillis;
    }

    /* ***********************                   SHOW IP ADDR           ************************/
    if (timeSlice.currentMillis - timeSlice.previousMillisShowIp > SHOW_IP_ADDR_INTERVALL)
    {
        tft_showIP(WiFi.localIP().toString().c_str());
        timeSlice.previousMillisShowIp = timeSlice.currentMillis;
    }

    /* ***********************                   FETCH TEMperature           ************************/

    if (timeSlice.currentMillis - timeSlice.previousMillTemp > TEMPERATURE_INTERVAL)
    {
        // LOG_INFO("TEMPERATURE_INTERVAL");
        if (!temp_getTemperature(webSockData.temperature))
        {

            if (webSockData.temperature.sensor1 < 0.0 && webSockData.temperature.sensor2 < 0.0)
            {
#ifdef MQTT
                mqtt_publish_alarm_temp(webSockData.temperature.sensor1, webSockData.temperature.sensor2);
#endif
                webSockData.temperature.alarm = true;
                if (!webSockData.temperature.alarm)
                {
                    LOG_ERROR("main::Temperatur Sensorik ausgefallen - Heizpatrone wird abgeschaltet");
                    pidPinManager.reset(); // alles aus
                    alarmContainer.alarmTemp.alarmTemp = true;
                    alarmContainer.alarmTemp.overFlowHappenedAt = time_getTimeStamp();
                    webSockData.temperature.alarm = true;
                }
            }
        }
        else
        {

            webSockData.temperature.alarm = false;
            /*
            RELAY_L1, RELAY_L2, PWM_FOR_PID
            */
            LOG_INFO("main::TEMP in Celsius, S1: %f, S2: %f", webSockData.temperature.sensor1, webSockData.temperature.sensor2);
            if (!alarmContainer.alarmTemp.alarmTemp)
            {
                if (((int)(webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2.0) > webSockData.setupData.tempMaxAllowedInGrad)
                {
                    LOG_ERROR( "main::Temperaturlimit erreicht - Heizpatrone wird abgeschaltet");

                    pidPinManager.reset(); // alles aus
                    alarmContainer.alarmTemp.alarmTemp = true;
                    alarmContainer.alarmTemp.overFlowHappenedAt = time_getTimeStamp();
                    webSockData.temperature.alarm = true;
                    ledHandler_showTemperaturError(true);
#ifdef MQTT
                    mqtt_publish_alarm_temp(webSockData.temperature.sensor1, webSockData.temperature.sensor2);
#endif
                }
            }
            else
            {
                time_t currT = time_getTimeStamp();
                double diffT = difftime(currT, alarmContainer.alarmTemp.overFlowHappenedAt); // in secs
                if (diffT > TEMPERATURE_OVERHEATED_WAIT_IN_SECS)
                {
                    LOG_INFO("main::TempLimit over %d °C , wait for next check in secs: %d", webSockData.setupData.tempMaxAllowedInGrad, TEMPERATURE_OVERHEATED_WAIT_IN_SECS);
                    if (((int)(webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2.0) > webSockData.setupData.tempMaxAllowedInGrad)
                    {
                        alarmContainer.alarmTemp.overFlowHappenedAt = time_getTimeStamp();
                        ledHandler_showTemperaturError(true);
#ifdef MQTT
                        mqtt_publish_alarm_temp(webSockData.temperature.sensor1, webSockData.temperature.sensor2);
#endif
                    }
                    else
                    {
                        alarmContainer.alarmTemp.alarmTemp = false;
                        alarmContainer.alarmTemp.overFlowHappenedAt = 0;
                        ledHandler_showTemperaturError(false);
                        LOG_INFO("main:: TempLimit reset");
                    }
                }
            }
        } // !getTemperature()
        timeSlice.previousMillTemp = timeSlice.currentMillis;
    }

    /* ***********************                   MODBUS           ************************/

    if ((webSockData.states.froniusAPI == true) && (webSockData.setupData.externerSpeicher == true && webSockData.states.networkOK == true))
    {

        if (timeSlice.currentMillis - timeSlice.previousMillisAkku > MODBUS_AKKU_INTERVALL)
        {

            if (webSockData.states.modbusOK)
            {
                if (!mb_readAkkuOnly(webSockData.setupData, webSockData.mbContainer))
                {
                    webSockData.states.modbusOK = false;
                    memset(&webSockData.mbContainer.akkuState, 0, sizeof(AKKU_STATE_VALUE_t));
                    memset(&webSockData.mbContainer.akkuStr, 0, sizeof(AKKU_STRG_VALUE_t));
                    ledHandler_showModbusError(true);
                }
            }
            else
            {
                LOG_INFO("main::webSockData.states.froniusAP:!mb_readAkkuOnl");
                memset(&webSockData.mbContainer.akkuState, 0, sizeof(AKKU_STATE_VALUE_t));
                memset(&webSockData.mbContainer.akkuStr, 0, sizeof(AKKU_STRG_VALUE_t));
            }

            /*      if (webSockData.states.froniusAPI)
                 {
                     // !!!!!!!! OVERRIDE MODBUS values with REST API values
                     if (solar_get_powerflow(webSockData.fronius_SOLAR_POWERFLOW))
                     {
                         webSockData.mbContainer.akkuStr.data.chargeRate = webSockData.fronius_SOLAR_POWERFLOW.p_akku;
                         webSockData.mbContainer.akkuStr.data.dischargeRate = webSockData.fronius_SOLAR_POWERFLOW.rel_Autonomy;
                         webSockData.mbContainer.akkuStr.data.maxChargeRate = webSockData.fronius_SOLAR_POWERFLOW.rel_SelfConsumption;
                     }
                 } */
            tft_drawInfo(webSockData);
            timeSlice.previousMillisAkku = timeSlice.currentMillis;
        }
    }

    if (timeSlice.currentMillis - timeSlice.previousMillModbus > MODBUS_INTERVALL)
    {
        // LOG_INFO("MODBUS_INTERVALL");

#ifdef FRONIUS_IV

        if (webSockData.states.froniusAPI == true && webSockData.states.networkOK == true)
        {
            LOG_INFO("main::webSockData.states.froniusAPI - solarAPI");
            int counter = 0;
            bool result = solar_get_powerflow(webSockData);
            while (!result && counter < MAX_RECONNECTING_NET)
            {
                counter++;
                delay(DELAY_RECONNECT_NET);
                result = solar_get_powerflow(webSockData);

                if (!result)
                {
                    LOG_ERROR("main::webSockData.states.froniusAPI - solarAPI - try to connect: %d, wait for %d secs reconnect .....", counter, DELAY_RECONNECT_NET / 1000);
                }
                else
                {
                    counter = MAX_RECONNECTING_NET + 1;
                }
            }

            if (result)
            {
                webSockData.mbContainer.akkuStr.data.chargeRate = webSockData.fronius_SOLAR_POWERFLOW.p_akku;
                webSockData.mbContainer.akkuStr.data.dischargeRate = webSockData.fronius_SOLAR_POWERFLOW.rel_Autonomy;
                webSockData.mbContainer.akkuStr.data.maxChargeRate = webSockData.fronius_SOLAR_POWERFLOW.rel_SelfConsumption;
                INVERTER_DATA.acCurrentPower = webSockData.fronius_SOLAR_POWERFLOW.p_akku + webSockData.fronius_SOLAR_POWERFLOW.p_pv;
                METER_DATA.acCurrentPower = webSockData.fronius_SOLAR_POWERFLOW.p_load;
            }
            else
            {
                webSockData.states.networkOK = false;
                return;
            }
        }
#endif
        if (!webSockData.states.froniusAPI == true && webSockData.states.modbusOK == true && webSockData.states.networkOK == true)
        {
            LOG_INFO("main::webSockData.states.modbus - modbus");
            if (mb_readInverter(webSockData.setupData, webSockData.mbContainer))
            {
                memset(formatBuffer, 0, FORMAT_CHAR_BUFFER_LEN);
                util_format_Watt_kWatt(INVERTER_DATA.acCurrentPower, formatBuffer); //  Produktion
                LOG_DEBUG("Produktion %s", formatBuffer);

                LOG_DEBUG("EXport %s", util_format_Watt_kWatt(METER_DATA.acCurrentPower, formatBuffer)); // Grid Bezug positiv, ansonst -

                if (METER_DATA.acCurrentPower < 0.0 && (INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower < 0))

                {
                    LOG_DEBUG("Wrong meter value from smartmeter - current production: %.3f !", METER_DATA.acCurrentPower);
#ifdef MQTT
                    mqtt_publish_modbus_wrong_production_val(METER_DATA.acCurrentPower);
#endif
                }
                else
                {
                    LOG_DEBUG("  in W: %s", util_format_Watt_kWatt(INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower, formatBuffer));
                    webSockData.pidContainer.mCurrentPower = METER_DATA.acCurrentPower; // export energy

                    /* #ifndef TEST_PID_WWWW
                                        availablePowerFromWRInWatt = webSockData.pidContainer.mCurrentPower;
                    #endif */

                    tft_drawInfo(webSockData);
                    influx_write(webSockData);
#ifdef MQTT

                    mqtt_publish_modbus_current_state(webSockData.mbContainer);
#endif
                }
            }
            else
            { // if readModbus
                LOG_INFO("MAIN::ModbusTimeSlice::  Error in reading modubs, network probably not ok, try to reconnect");
                webSockData.states.networkOK = false;
            }
        }
        if (!webSockData.states.amisReader)
        {
            webSockData.amisReader.saldo = 0;
        }
#ifdef AMIS_READER_DEV
        else
        {
            if (timeSlice.currentMillis - timeSlice.previousMillisAmis > AMIS_READER_INTERVALL)
            {
                if (webSockData.states.networkOK == true)
                {
                    if (amisReader_readRestTarget(webSockData))
                    {
                        LOG_INFO("main:: AmisReader: available is: %d, import: %d , export: %ld", webSockData.amisReader.saldo, webSockData.amisReader.absolutImportInkWh, webSockData.amisReader.absolutExportInkWh);
                        METER_DATA.acCurrentPower = webSockData.amisReader.saldo; // grid bezug
                    }
                    else
                    {
                        LOG_ERROR("main::AmisReader data not available - network error?");
                        webSockData.states.networkOK = false;
                    }
                }
                timeSlice.previousMillisAmis = timeSlice.currentMillis;
            }
        }
#endif

        timeSlice.previousMillModbus = timeSlice.currentMillis;
    }

    /* ***********************                   FLUSH LOGGING FILE           ************************/
#ifdef CARD_READER
    if (timeSlice.currentMillis - timeSlice.previousMillModbus > LOGGING_FLUSH_INTERVALL)
    {
        LOG_INFO("LOGGING_FLUSH_INTERVALL");
        if (webSockData.states.cardWriterOK)
        {

            cardRW_flushLoggingFile();
            timeSlice.previousMillModbus = timeSlice.currentMillis;
            cardRW_closeLoggingFile();
        }
        else
        {
            DBG("main:: flush logging - cannot flush!");
        }
    }
#endif
    /* ***********************                   PID CONTROLLER           ************************/

    if (timeSlice.currentMillis - timeSlice.previousMillisController > PID_CONTROLLER_INTERVALL)
    {
        LOG_DEBUG("main::PID_CONTROLLER_INTERVALL");
        if (webSockData.states.networkOK)
        {
            if (!alarmContainer.alarmTemp.alarmTemp)
            {

                pidPinManager.task(webSockData);
                webSockData.pidContainer.mAnalogOut = pidPinManager.getStateOfAnaPin();
                webSockData.pidContainer.PID_PIN1 = pidPinManager.getStateOfDigPin(0); // PIN 1
                webSockData.pidContainer.PID_PIN2 = pidPinManager.getStateOfDigPin(1); // PIN 2
            }
            else
            {
                LOG_WARNING("main::Temperature alarm container is on (%d)", alarmContainer.alarmTemp.alarmTemp);
            }
        }

        timeSlice.previousMillisController = timeSlice.currentMillis;
        // LOG_INFO("PID_CONTROLLER_INTERVALL");
    }

    if (timeSlice.currentMillis - timeSlice.previousMillisHeapCheck > CHECK_HEAP_SIZE_INTERVALL)
    {
        LOG_DEBUG("CHECK_HEAP_SIZE_INTERVALL)");
        heapSize[1].heapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        heapSize[1].heapSizeMax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        LOG_DEBUG("Free heap prev: %d Free heap now: %d, diff: %d largest block: %d", heapSize[0].heapSize, heapSize[1].heapSize, heapSize[1].heapSize - heapSize[0].heapSize, heapSize[1].heapSizeMax);

        timeSlice.previousMillisHeapCheck = timeSlice.currentMillis;
#ifdef TEST_PID_WWWW1
        Setup d;
        eprom_getSetup(d);
        // eprom_show(d);
        if (eprom_stammDataUpdate())
        {
            LOG_INFO("main PID-TEST update");
            availablePowerFromWRInWatt = webSockData.setupData.forceHeating = d.forceHeating;

            webSockData.pidContainer.mCurrentPower = d.forceHeating * 1.00;
            LOG_INFO("PID-TEST (1): available watt: %d", d.forceHeating);
            eprom_stammDataUpdateReset();
        }

        webSockData.setupData.pid_p = d.pid_p;
        webSockData.setupData.pid_d = d.pid_d;
        webSockData.setupData.pid_i = d.pid_i;
#ifdef MQTT
        mqtt_publish_pidParams(webSockData.setupData.pid_p, webSockData.setupData.pid_i, webSockData.setupData.pid_d);
#endif

#endif
    }

    if (timeSlice.currentMillis - timeSlice.previousMillisWebSocks > WEBSOCK_NOTIFY_INTERVALL)
    {
        LOG_INFO("WEBSOCK_NOTIFY_INTERVALL");
        timeSlice.previousMillisWebSocks = timeSlice.currentMillis;
        notifyClients(getJsonObj());
    }

    /*  if (timeSlice.currentMillis - timeSlice.previousMillisReconnect > RECONNET_INTERVALL)
     {
         LOG_INFO("RECONNET_INTERVALL");
         if (!wifi_isStillConnected(webSockData.setupData))
         {
             DBG("main::Network down , try reconnecting ....");
             webSockData.states.networkOK = false;
         }
         else
         {
             webSockData.states.networkOK = true;
         }

         timeSlice.previousMillisReconnect = timeSlice.currentMillis;
     } */
    // SETUP_CHECK_INTERVALL
    if (timeSlice.currentMillis - timeSlice.previousMillisSetup > SETUP_CHECK_INTERVALL)
    {
        LOG_INFO("main::SETUP_CHECK_INTERVALL %d ", webSockData.setupData.setupChanged);
        if (webSockData.setupData.setupChanged)
        {
            if (!hotUpdate(webSockData, pidPinManager))
            {
                LOG_INFO("main::SETUP_CHECK_INTERVALL Restarti");
                delay(5000);
                esp_restart();
            }
            webSockData.setupData.setupChanged = false;
        }
        timeSlice.previousMillisSetup = timeSlice.currentMillis;
    }
    // logReader_captureSerialOutput();
    //  eM_setSleepTime(20);
    //  eM_lightSleep();

} // loop

WEBSOCK_DATA &getDataForWebSocket()
{
    // LOG_INFO("getDataForWebSocket, Temp: %.2lf", webSockData.temperature.sensor1);
    return webSockData;
}
bool &setSetupChanged(bool didSetupChanged)
{
    webSockData.setupData.setupChanged = didSetupChanged;
    return webSockData.setupData.setupChanged;
}