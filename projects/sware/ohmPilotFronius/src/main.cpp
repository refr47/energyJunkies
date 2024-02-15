#include <Arduino.h>
#include "esp_clk.h"
#include <SPI.h>
#include <esp_log.h>

#include "debugConsole.h"
#include "wlan.h"
#ifdef FRONIUS_IV
#include "modbusReader.h"
#elif HUAWEI_IV
#include "huawei.h"
#endif

#include "cardRW.h"
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
#ifdef MQTT
#include "mqtt.h"
#endif
#ifdef FRONIUS_API
#include "froniusSolarAPI.h"
#endif
#ifdef INFLUX
#include "influx.h"
#endif
#ifdef AMIS_READER_DEV
#include "amisReader.h"
#endif
/*
Input only pins
GPIOs 34 to 39 are GPIs – input only pins. These pins don’t have internal pull-up or pull-down resistors. They can’t be used as outputs, so use these pins only as inputs:


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
#define MODBUS_AKKU_INTERVALL 5000
#define CLOCK_INTERVALL 1000           // secs
#define WEBSOCK_NOTIFY_INTERVALL 10000 // 5 secs
#define SHOW_IP_ADDR_INTERVALL 5000
#define CONFIG_PARAM_TEST_INTERVALL 6000
#define AMIS_READER_INTERVALL 10000
#define FORMAT_CHAR_BUFFER_LEN 50 // @see loop

#ifndef TAG
#define TAG "E-JUNKIES"
#endif
#define LOG_LEVEL ESP_LOG_INFO
#define MY_ESP_LOG_LEVEL ESP_LOG_INFO

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

typedef struct _TIME_SLICE
{
    unsigned long previousMillTemp;
    unsigned long previousMillModbus;
    unsigned long previousMillFlush;
    unsigned long previousMillisClock;
    unsigned long previousMillisWebSocks;
    unsigned long previousMillisShowIp;
    unsigned long previousMillisController;
    unsigned long previousMillisTestConfig;
    unsigned long previousMillisAkku;
    unsigned long previousMillisAmis;
    unsigned long currentMillis;

} TIME_SLICE;

/* ****************************************************************************
  GLOBAL VARS https://randomnerdtutorials.com/esp32-websocket-server-sensor/
  ****************************************************************************
*/

char globalStringBuffer[GLOBAL_STRING_BUFFER_LEN];
static bool networkCredentialsInEEprom = true;

static TIME_SLICE timeSlice;

static LIFE_DATA lifeData;
static ALARM_CONTAINER alarmContainer;
static PinManager pidPinManager;
static WEBSOCK_DATA webSockData;
static double availablePowerFromWRInWatt = 0.0;

/* **************************************************************************
        ProtoTypes
*/

WEBSOCK_DATA &getDataForWebSocket();

// int sdCardLogOutput(const char *format, va_list args); // LOG-System

// https://community.platformio.org/t/redirect-esp32-log-messages-to-sd-card/33734/5
void logging_init()
{

    DBGf("Setting log levels and callback");
    esp_log_level_set("*", MY_ESP_LOG_LEVEL);
    esp_log_level_set(TAG, LOG_LEVEL);
    esp_log_set_vprintf(cardRW_LogOutput);
    if (!cardRW_createLoggingFile())
    {
        ESP_LOGE(TAG, "Cannot create logging file on sd card");
    }
}

void test_cardReader()
{
    cardRW_listDir("/", 3);
}

#ifdef EJ

int s1_enter = 1;
int s2_up = 1;
int s3_down = 1;
int s4_esc = 1;

#endif

void setup()
{

    DBGbgn(115200);
    while (!Serial)
        ;

    DBGf("Energie-Junkies -- Harvester ---");
    memset(&webSockData, 0, sizeof(WEBSOCK_DATA));
    // memset(&states, 0, sizeof(STATES));
    ledHandler_init();
    tft_init();
    tft_printSetup();

    /*  int currentState = digitalRead(INTERNAL_BUTTON_2_GPIO);
     DBGf("Interal button: %d", currentState); */

    /* uint32_t cpu_freq = esp_clk_cpu_freq();
    DBG(" CPU freq: ");
    DBGln(cpu_freq);
    uint32_t PRESCALE = 240; // for 240MHZ */

    // eprom_test_write_Eprom("Milchbehaelter", "47754775");
    //    eprom_clearLifeData();
    eprom_isInit();

    // eprom_test_write_Eprom("Milchbehaelter", "47754775");
    eprom_getSetup(webSockData.setupData);
    // eprom_getLifeData(lifeData);

    // printEprom(webSockData.setupData);
    //  eprom_show(webSockData.setupData);
    DBGf("get network   ");
    // if (strcmp(webSockData.setupData.ssid, "---") == 0)
    if (strcmp(webSockData.setupData.ssid, "--") == 0)
    {
        networkCredentialsInEEprom = false;
        webSockData.states.networkOK = false;
        www_init(NULL, NULL, getDataForWebSocket); // act as access point
    }
    if (networkCredentialsInEEprom)
    {

        /*       char buff[130];
              memset(buff, 0, strlen(buff)); */

        if (!wifi_init(webSockData.setupData))
        {
            DBGf("Cannot connect - show available networks: ");
            // tft_drawNetworkInfo(NULL, webSockData.setupData.ssid);
            tft_clearScreen();
            wifi_scan_network();
            www_init(NULL, NULL, getDataForWebSocket); // act as access point
            webSockData.states.networkOK = false;
        }
        else
        {

            char *pBuf = globalStringBuffer;
            wifi_getLocalIP(&pBuf);
            DBGf("Connected with ip: %s", globalStringBuffer);

            tft_drawNetworkInfo(globalStringBuffer, webSockData.setupData.ssid);
            webSockData.states.flashOK = www_init(pBuf, webSockData.setupData.ssid, getDataForWebSocket); // do not act as apoint
            webSockData.states.networkOK = true;
        }

        if (!webSockData.states.networkOK)
        {
            DBGf("Network does not work!");
            // tft_printInfo("No valid network!");
            return;
        }

        // tft_printInfo("       ");
        tft_printKeyValue("Init Time", "OK", TFT_GREEN);
        time_init(); // init time
        time_currentTimeStamp();
        DBGf("Setup Modbus ...");
        if (!mb_init(webSockData.setupData))
        {
            DBGf("Cannot initialize modbus ....");
            tft_printKeyValue("Init mdobus", "Error", TFT_RED);
            webSockData.states.modbusOK = false;
            ledHandler_showModbusError(true);
        }
        else
        {
            webSockData.states.modbusOK = true;
            tft_printKeyValue("Init Modbus", "ok", TFT_GREEN);
        }

        // memset(&webSockData.mbContainer, 0, sizeof(MB_CONTAINER));
#ifdef EJ

        delay(3000);
        tft_clearScreen();
    }
#endif
#ifndef EJ

#ifdef MQTT
    if (!mqtt_init())
    {
        DBGf("Mqtt-Server -- cannot connect (!)");
    }
    else
        DBGf("Mqtt-Server:: connected successfully ...");
#endif
    webSockData.states.cardWriterOK = false;
    if (cardRW_setup(false, false))
    {
        webSockData.states.cardWriterOK = true;
        tft_printKeyValue("Init CardReader", "OK", TFT_GREEN);

        logging_init();
        // test_cardReader();
    }
    else
    {
        DBGf("Logging to file cannot be initiated ...");
        tft_printKeyValue("Init CardReader", "Error", TFT_RED);

        ledHandler_showCardReaderError(true);
    }

    if (temp_init())
    {

        tft_printKeyValue("Init Sensors", "OK", TFT_GREEN);
        webSockData.states.tempSensorOK = true;
    }
    else
    {
        webSockData.states.tempSensorOK = false;
        tft_printKeyValue("Init Sensors", "Error", TFT_RED);
    }
#ifdef WEATHER_API
    // wheater_getForecast();
#endif
    if (!mb_readAll(webSockData.setup, webSockData.mbContainer))
    {
        DBGf("Init::readAllModbusValues did not succeed");
        tft_printKeyValue("Modbus Read", "Error", TFT_RED);
    }

    webSockData.states.froniusAPI = false;

#ifdef FRONIUS_API
    bool akkuAvailable = false;
    DBGf("main::before solar_init");
    try
    {
        printEprom(webSockData.setup);
    }
    catch (const std::exception &e)
    {
        Serial.println("Exception occured:");
    }

    DBGf("main::before solar_init 2, tcp as uint: %d", webSockData.setup.ipInverter);
    const char *cp = webSockData.setup.inverterAsString;
    if (cp == NULL)
        DBGf("cp is null");
    else
        DBGf("CP is: %s", cp);
    Serial.println(webSockData.setup.inverterAsString);
    DBGf("inverter: %s", webSockData.setup.inverterAsString);
    if (soloar_init(webSockData, &akkuAvailable))
    {
        webSockData.setup.externerSpeicher = akkuAvailable;
        DBGf("main - akku: %d", webSockData.setup.externerSpeicher);
        memset(&webSockData.fronius_SOLAR_POWERFLOW, 0, sizeof(FRONIUS_SOLAR_POWERFLOW));
        if (solar_get_powerflow(webSockData))
        {
            webSockData.states.froniusAPI = true;
            DBGf("main1 - akku: %d", webSockData.setup.externerSpeicher);
            tft_printKeyValue("Fronius Solar API", "Yes", TFT_GREEN);
            if (webSockData.setup.externerSpeicher == true)
                tft_printKeyValue("AKKU", "Yes", TFT_GREEN);
            else
                tft_printKeyValue("AKKU", "No", TFT_GREEN);
            DBGf("Fronius solar API Support: YES, AKKU: %s", webSockData.setup.externerSpeicher == true ? "Yes" : "NO");
        }
        else
        {
            DBGf("setup::solar_init - Cannot access Fronius REST  SOLAR API form converter!");
            tft_printKeyValue(" Fronius Solar API", "ERROR", TFT_RED);
        }
    }

#endif

    DBGf("Setup PID-Controller");
#ifdef MQTT
    mqtt_publish_pidParams(webSockData.setupData.pid_p, webSockData.setupData.pid_i, webSockData.setupData.pid_d);
#endif
    // DBGf("Mqtt - PID params:  p: %.2lf  i: %.2lf    d: %.2lf", webSockData.setupData.pid_p, webSockData.setupData.pid_i, webSockData.setupData.pid_d);
    tft_printKeyValue("Init PID-Manager", "ok", TFT_GREEN);
    pidPinManager.config(webSockData.setupData, RELAY_L1, RELAY_L2, PWM_FOR_PID);
#ifdef INFLUX
    webSockData.states.influx = false;
    if (influx_init(webSockData))
    {
        webSockData.states.influx = true;
    }
#endif
    webSockData.states.amisReader = false;
#ifdef AMIS_READER_DEV
    if (amisReader_initRestTargets(webSockData.setupData))
    {
        webSockData.states.amisReader = true;

        if (amisReader_readRestTarget(webSockData))
        {
            webSockData.states.amisReader = true;
            tft_printKeyValue("Init AMIS-Reader", "ok", TFT_GREEN);
            DBGf("AMIS-Reader:: connected successfully ...");
        }
        else
        {

            tft_printKeyValue("Init AMIS-Reader", "Error", TFT_RED);
            DBGf("AMIS-Reader:: connection failed ...");
        }
    }
    else
    {
        tft_printKeyValue("NO AMIS-Reader", "Error", TFT_RED);
        DBGf("amisReader not found");
    }
}
#endif
if (webSockData.states.networkOK)
{
    delay(5000);

    DBGf(" -------- States ---------------");
    DBGf("Fronius: %c", webSockData.states.froniusAPI == true ? 'y' : 'n');
    DBGf("AmisReader: %c", webSockData.states.amisReader == true ? 'y' : 'n');
    DBGf("CardWrite: %c", webSockData.states.cardWriterOK == true ? 'y' : 'n');
    DBGf("FlashFS: %c", webSockData.states.flashOK == true ? 'y' : 'n');
    DBGf("Influx: %c", webSockData.states.influx == true ? 'y' : 'n');
    DBGf("Modbus: %c", webSockData.states.influx == true ? 'y' : 'n');
    DBGf("MqTT: %c", webSockData.states.mqtt == true ? 'y' : 'n');
    DBGf("TempSensor: %c", webSockData.states.tempSensorOK == true ? 'y' : 'n');
    tft_clearScreen();
}
ESP_LOGI(TAG, "Setup done - all components are working...");
#else
        ESP_LOGI(TAG, "Start testing...");

        pinMode(SWITCH_KEY_ENTER, INPUT_PULLUP);
        /* pinMode(SWITCH_KEY_UP, INPUT_PULLUP);
        pinMode(SWITCH_KEY_DOWN, INPUT_PULLUP); */
        pinMode(SWITCH_KEY_ESC, INPUT_PULLUP);

        pinMode(RELAY_L1, OUTPUT);
        pinMode(RELAY_L2, OUTPUT);
        pinMode(PWM_FOR_PID, OUTPUT);
        memset(&timeSlice, 0, sizeof(timeSlice));

#endif
}

static char formatBuffer[FORMAT_CHAR_BUFFER_LEN];

#ifdef EJ
static int pwmValue = 0;
static uint8_t l1 = false, l2 = false;
static bool pressedEnter = false;

static int s1_esc_prev = 1, s2_enter_prev = 1;
static time_t time1, time2, time3, time4;
static bool startMe = false, stopMe = false, initMe = true, doneStop = true;
static double currentConsumeInWatt, accumulatedWatt = 0.0;

#define MAX_VAL 1700.0

#endif
void loop()
{

#ifdef EJ

    s1_enter = digitalRead(SWITCH_KEY_ENTER);
    /*    s2_up = digitalRead(SWITCH_KEY_UP);
      s3_down = digitalRead(SWITCH_KEY_DOWN);  */
    s4_esc = digitalRead(SWITCH_KEY_ESC);

    if ((s4_esc == LOW) && (s1_esc_prev == 1))
    {
        delay(40);
        s4_esc = digitalRead(SWITCH_KEY_ESC);
        // DBGf("ENER s4, pwm: %d, l1: %x", pwmValue, l1);
        if (s4_esc == LOW)
        {

            pwmValue = 0;
            stopMe = true;
            startMe = false;
            initMe = false;
            doneStop = false;
            l1 = !l1;
        }
        s1_esc_prev = s4_esc;
        // DBGf("exit s4, pwm: %d, l1: %x", pwmValue, l1);
    }
    if ((s1_enter == LOW) && (s2_enter_prev == 1))
    {
        delay(40);
        s1_enter = digitalRead(SWITCH_KEY_ENTER);
        // DBGf("ENtER s1-ENTER, pwm: %d, l2: %x", pwmValue, l2);
        if (s1_enter == LOW)
        {

            pwmValue = 255;
            accumulatedWatt = 0.0;
            stopMe = false;
            doneStop = false;
            startMe = true;
            initMe = false;
            DBGf("enter: startMe: %s, initMe: %s", startMe == true ? "true" : "false", initMe == true ? "true" : "false");
            // DBGf("Set pwm to %d", pwmValue);

            l2 = !l2;
        }
        s2_enter_prev = s1_enter;
    }

    timeSlice.currentMillis = millis();

    if (timeSlice.currentMillis - timeSlice.previousMillisClock > CLOCK_INTERVALL)
    {

        s1_esc_prev = 1;
        s2_enter_prev = 1;
        sprintf(formatBuffer, "%d", webSockData.setupData.heizstab_leistung_in_watt);
        tft_print_test(3, 15, 150, TFT_BLUE, "Heizstab", formatBuffer);
        tft_print_test(4, 15, 150, TFT_BLUE, "Enter -", "ESC +");

        sprintf(formatBuffer, "%d", pwmValue);
        tft_print_test(5, 15, 130, TFT_GREEN, "PWM", formatBuffer);
        // DBGf("enter: startMe: %s, initMe: %s", startMe == true ? "true" : "false", initMe == true ? "true" : "false");
        if (startMe)
        {
            if (initMe == false)
            {
                DBGf("Start Messung & init");

                sprintf(formatBuffer, "0");

                if (mb_readSmartMeterAndInverterOnly(webSockData.setupData, webSockData.mbContainer))
                {
                    tft_print_test(11, 15, 150, TFT_RED, "Error", "Modbus cannot read");
                    DBGf("Error reading modbus");
                    return;
                }
                else
                {
                    tft_print_test(11, 15, 150, TFT_RED, "Error", "None");
                    time(&time1);
                    currentConsumeInWatt = INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower;
                    sprintf(formatBuffer, "V %+6.1lf |W %+6.1lf |S %+6.1lf", currentConsumeInWatt, INVERTER_DATA.acCurrentPower, METER_DATA.acCurrentPower);
                    DBGf("SMeter (start) %s", formatBuffer);
                    tft_print_test(6, 15, 110, TFT_GREEN, "Start (W) ", formatBuffer);
                    if (currentConsumeInWatt == 0.0)
                    {
                        DBGf("Smart Meter returns value 0");
                    }
                    else
                    {
                        initMe = true;
                        DBGf("Smart Meter returns value !=0");
                    }
                    analogWrite(PWM_FOR_PID, pwmValue);
                }
                tft_print_test(8, 15, 130, TFT_GREEN, "Stop (secs) ", "0");
            }
            else
            {
                // DBGf("Start Messung & Modbus read");
                time(&time3);
                if (mb_readSmartMeterAndInverterOnly(webSockData.setupData, webSockData.mbContainer))
                {
                    tft_print_test(11, 15, 150, TFT_RED, "Error", "Modbus cannot read");
                    DBGf("Error reading modbus");
                }
                else
                {
                    time(&time4);
                    double dTime = difftime(time4, time3);
                    sprintf(formatBuffer, "%.1f secs", dTime);
                    tft_print_test(9, 15, 150, TFT_GREEN, "ModB Call (sec)", formatBuffer);
                    dTime = difftime(time4, time1);
                    tft_print_test(11, 15, 150, TFT_RED, "Error", "None");
                    accumulatedWatt += INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower - currentConsumeInWatt;
                    sprintf(formatBuffer, "%0.1lf V %+6.1f |W %+6.1f |S %+6.1f", dTime, INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower - currentConsumeInWatt, INVERTER_DATA.acCurrentPower, METER_DATA.acCurrentPower);
                    DBGf("Values: %s", formatBuffer);
                    tft_print_test(7, 15, 80, TFT_GREEN, "Diff (W) ", formatBuffer);
                    DBGf("Diff  %s", formatBuffer);
                    if (INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower - currentConsumeInWatt >= MAX_VAL)
                    {
                        pwmValue = 0;
                        stopMe = true;
                        startMe = false;
                        initMe = false;
                        doneStop = false;
                    }
                }
            }
        }
        else
        {
            if (stopMe && !doneStop)
            {
                DBGf("Stop Messung");
                analogWrite(PWM_FOR_PID, pwmValue);
                time(&time2);
                delay(1000);
                mb_readSmartMeterAndInverterOnly(webSockData.setupData, webSockData.mbContainer);

                double dTime = difftime(time2, time1);
                sprintf(formatBuffer, "%.1f secs", dTime - 1.0);
                doneStop = true;
                DBGf("Stop Messung within %f secs", dTime);
                sprintf(formatBuffer, "%0.1lf %+6.1f | %+6.1f | %+6.1f", dTime, INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower - currentConsumeInWatt, INVERTER_DATA.acCurrentPower, METER_DATA.acCurrentPower);

                tft_print_test(7, 15, 80, TFT_GREEN, "Diff (W) ", formatBuffer);
                DBGf("Stop - last values: %s", formatBuffer);
                sprintf(formatBuffer, "Time: %.1f", dTime);
                tft_print_test(8, 15, 130, TFT_GREEN, "Stop (secs) ", formatBuffer);
                mb_readSmartMeterAndInverterOnly(webSockData.setupData, webSockData.mbContainer);
            }
            else
            {
                /* getCurrentTime(formatBuffer, sizeof(formatBuffer));
                tft_print_test(9, 15, 130, TFT_GREEN, "Time ", formatBuffer); */
            }
        }

        // tft_print_test(6, 15, 130, TFT_GREEN, "TimeS ", formatBuffer);

        /*  digitalWrite(RELAY_L1, l1);
        tft_print_test(6, 15, 130, TFT_GREEN, "L1", l1 == LOW ? "LOW" : "HIGH");
        digitalWrite(RELAY_L2, l2);
        tft_print_test(7, 15, 130, TFT_GREEN, "L2", l2 == LOW ? "LOW" : "HIGH"); */
        DBGf("L1: %d  L2: %d    PWM: %d", l1, l2, pwmValue);

        // modbus
        timeSlice.previousMillisClock = timeSlice.currentMillis;
    }

#endif

#ifndef EJ
    if (!webSockData.states.networkOK)
    {
        DBGf("Network does not work - no further task are available...");
        delay(10000);
        return;
    }

    timeSlice.currentMillis = millis();
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
        // DBGf("TEMPERATURE_INTERVAL");
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
                    ESP_LOGE(TAG, "Temperatur Sensorik ausgefallen - Heizpatrone wird abgeschaltet");
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
            DBGf(" TEMP in Celsius, S1: %f, S2: %f", webSockData.temperature.sensor1, webSockData.temperature.sensor2);
            if (!alarmContainer.alarmTemp.alarmTemp)
            {
                if (((int)(webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2.0) > webSockData.setupData.tempMaxAllowedInGrad)
                {
                    ESP_LOGE(TAG, "Temperaturlimit erreicht - Heizpatrone wird abgeschaltet");

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
                    DBGf("TempLimit over %d °C , wait for next check in secs: %d", webSockData.setupData.tempMaxAllowedInGrad, TEMPERATURE_OVERHEATED_WAIT_IN_SECS);
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
                        alarmContainer.alarmTemp.alarmTemp = true;
                        alarmContainer.alarmTemp.overFlowHappenedAt = 0;
                        ledHandler_showTemperaturError(false);
                        DBGf("TempLimit reset");
                    }
                }
            }
        } // !getTemperature()
        timeSlice.previousMillTemp = timeSlice.currentMillis;
    }

    /* ***********************                   MODBUS           ************************/

    if ((webSockData.states.froniusAPI == true) && (webSockData.setup.externerSpeicher == true))
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
                DBGf("main::webSockData.states.froniusAP:!mb_readAkkuOnl");
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
        // DBGf("MODBUS_INTERVALL");

        if (!webSockData.states.modbusOK)
        {
            tft_drawInfoNoModbus(webSockData.temperature);
            eprom_getSetup(webSockData.setupData); // reRead setup data from eprom - maybe changed by web
            if (mb_init(webSockData.setupData))
            {
                DBGf("Reconnected modbus successfully.");
                ledHandler_showModbusError(false);
                webSockData.states.modbusOK = true;
#ifdef MQTT
                mqtt_publish_modbus_reconnect(webSockData.setupData.inverterAsString.c_str());
#endif
            }
        }
        else
        {
#ifdef FRONIUS_API

            if (webSockData.states.froniusAPI)
            {

                if (solar_get_powerflow(webSockData))
                {
                    webSockData.mbContainer.akkuStr.data.chargeRate = webSockData.fronius_SOLAR_POWERFLOW.p_akku;
                    webSockData.mbContainer.akkuStr.data.dischargeRate = webSockData.fronius_SOLAR_POWERFLOW.rel_Autonomy;
                    webSockData.mbContainer.akkuStr.data.maxChargeRate = webSockData.fronius_SOLAR_POWERFLOW.rel_SelfConsumption;
                    INVERTER_DATA.acCurrentPower = webSockData.fronius_SOLAR_POWERFLOW.p_akku + webSockData.fronius_SOLAR_POWERFLOW.p_pv;
                    METER_DATA.acCurrentPower = webSockData.fronius_SOLAR_POWERFLOW.p_load;
                  
                }
            }
#endif
            else
            {
                if (mb_readInverter(webSockData.setupData, webSockData.mbContainer))
                {

                    memset(formatBuffer, 0, FORMAT_CHAR_BUFFER_LEN);
                    util_format_Watt_kWatt(INVERTER_DATA.acCurrentPower, formatBuffer); //  Produktion
                    DBGf("Produktion %s", formatBuffer);

                    DBGf("EXport %s", util_format_Watt_kWatt(METER_DATA.acCurrentPower, formatBuffer)); // Grid Bezug positiv, ansonst -

                    if (METER_DATA.acCurrentPower < 0.0 && (INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower < 0))

                    {
                        DBGf("Wrong meter value from smartmeter - current production: %f !", METER_DATA.acCurrentPower);
#ifdef MQTT
                        mqtt_publish_modbus_wrong_production_val(METER_DATA.acCurrentPower);
#endif
                    }
                    else
                    {
                        DBGf("  in W: %s", util_format_Watt_kWatt(INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower, formatBuffer));
                        webSockData.pidContainer.mCurrentPower = METER_DATA.acCurrentPower; // export energy
                       

#ifndef TEST_PID_WWWW
                        availablePowerFromWRInWatt = webSockData.pidContainer.mCurrentPower;
#endif

                        tft_drawInfo(webSockData);
                        influx_write(webSockData);
#ifdef MQTT

                        mqtt_publish_modbus_current_state(webSockData.mbContainer);
#endif
                    }
                }
                else
                { // if readModbus
                    DBGf("MAIN::ModbusTimeSlice::  Error in reading modubs");
                }
            }
        } // if (!webSockData.states.modbusOK

        timeSlice.previousMillModbus = timeSlice.currentMillis;
    } // if

    if (!webSockData.states.amisReader)
    {
        webSockData.amisReader.saldo = 0;
    }
    else
    {
        if (timeSlice.currentMillis - timeSlice.previousMillisAmis > AMIS_READER_INTERVALL)
        {

            if (amisReader_readRestTarget(webSockData))
            {
                DBGf("main:: AmisReader: available is: %d, import: %d , export: %d", webSockData.amisReader.saldo, webSockData.amisReader.absolutImportInkWh, webSockData.amisReader.absolutExportInkWh);
                 METER_DATA.acCurrentPower = webSockData.amisReader.saldo; // grid bezug
            }
            else
            {
                DBGf("main::AmisReader data not available.");
            }
            timeSlice.previousMillisAmis = timeSlice.currentMillis;
        }
    }

    /* ***********************                   FLUSH LOGGING FILE           ************************/

    if (timeSlice.currentMillis - timeSlice.previousMillModbus > LOGGING_FLUSH_INTERVALL)
    {
        // DBGf("LOGGING_FLUSH_INTERVALL");
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
    /* ***********************                   PID CONTROLLER           ************************/

    if (timeSlice.currentMillis - timeSlice.previousMillisController > PID_CONTROLLER_INTERVALL)
    {
        DBGf("PID_CONTROLLER_INTERVALL");

        if (!alarmContainer.alarmTemp.alarmTemp)
        {

            // pidPinManager.task(webSockData);
            webSockData.pidContainer.mAnalogOut = pidPinManager.getStateOfAnaPin();
            webSockData.pidContainer.PID_PIN1 = pidPinManager.getStateOfDigPin(0); // PIN 1
            webSockData.pidContainer.PID_PIN2 = pidPinManager.getStateOfDigPin(1); // PIN 2

            /*  if (availablePowerFromWRInWatt < 0.0) // energy export
             {
                 // DBGf(" main::Einspeisung %lf, muss übrig bleiben %d", availablePowerFromWRInWatt, webSockData.setupData.pid_powerWhichNeedNotConsumed);

                 //  Einspeisung - Wieviel müss übrig bleiben
                 pidPinManager.task(webSockData.setupData, &availablePowerFromWRInWatt);
             }
             else
             {

                 // DBGf(" main::Bezug  %f", availablePowerFromWRInWatt);

                 pidPinManager.task(webSockData.setupData, &availablePowerFromWRInWatt);
             } */
        }
        timeSlice.previousMillisController = timeSlice.currentMillis;
        // DBGf("PID_CONTROLLER_INTERVALL");
    }

    // delay(2000);
    // DBGf(" main:: available watt: %f", availablePowerFromWRInWatt);

    /* ***********************                   CONFIG LIVE UPDATE sTAMMdaTEN via WEB           ************************/
    if (timeSlice.currentMillis - timeSlice.previousMillisTestConfig > CONFIG_PARAM_TEST_INTERVALL)
    {
        mb_readAkkuOnly(webSockData.setup, webSockData.mbContainer);
        // DBGf("CONFIG_PARAM_TEST_INTERVALL");
        timeSlice.previousMillisTestConfig = timeSlice.currentMillis;
#ifdef TEST_PID_WWWW1
        Setup d;
        eprom_getSetup(d);
        // eprom_show(d);
        if (eprom_stammDataUpdate())
        {
            DBGf("main PID-TEST update");
            availablePowerFromWRInWatt = webSockData.setup.exportWatt = d.exportWatt;

            webSockData.pidContainer.mCurrentPower = d.exportWatt * 1.00;
            DBGf("PID-TEST (1): available watt: %d", d.exportWatt);
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
        // DBGf("WEBSOCK_NOTIFY_INTERVALL");
        timeSlice.previousMillisWebSocks = timeSlice.currentMillis;
        notifyClients(getJsonObj());
    }
    if (networkCredentialsInEEprom == false)
    { // act as AP
        www_run();
    }

#endif

} // loop

WEBSOCK_DATA &getDataForWebSocket()
{
    // DBGf("getDataForWebSocket, Temp: %.2lf", webSockData.temperature.sensor1);
    return webSockData;
}
