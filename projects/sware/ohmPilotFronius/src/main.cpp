#include <Arduino.h>
#include "esp_clk.h"
#include <SPI.h>
#include <esp_log.h>

#include "debugConsole.h"
#include "wlan.h"
#include "modbusReader.h"
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
#ifdef MQTT
#include "mqtt.h"
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
#define MODBUS_INTERVALL 1000UL
#define PID_CONTROLLER_INTERVALL 100 // 0.1 secs default sample time
#define LOGGING_FLUSH_INTERVALL 60000
#define CLOCK_INTERVALL 1000           // secs
#define WEBSOCK_NOTIFY_INTERVALL 10000 // 5 secs
#define SHOW_IP_ADDR_INTERVALL 5000
#define CONFIG_PARAM_TEST_INTERVALL 2000

#define FORMAT_CHAR_BUFFER_LEN 50 // @see loop

#ifndef TAG
#define TAG "E-JUNKIES"
#endif
#define LOG_LEVEL ESP_LOG_INFO
#define MY_ESP_LOG_LEVEL ESP_LOG_INFO

#define INVERTER_DATA webSockData.mbContainer.inverterSumValues.data
#define METER_DATA webSockData.mbContainer.meterValues.data

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
    unsigned long currentMillis;

} TIME_SLICE;

/* ****************************************************************************
  GLOBAL VARS https://randomnerdtutorials.com/esp32-websocket-server-sensor/
  ****************************************************************************
*/

char globalStringBuffer[GLOBAL_STRING_BUFFER_LEN];
static bool networkCredentialsInEEprom = true;

/* static bool cardWriterOK = false;
static bool networkOK = false; */
// static STATES states;

static TIME_SLICE timeSlice;
static Setup setupData;
static LIFE_DATA lifeData;
static ALARM_CONTAINER alarmContainer;
static PinManager pidPinManager;
static WEBSOCK_DATA webSockData;
static double availablePowerFromWRInWatt = 0.0;

/* **************************************************************************
        ProtoTypes
*/

WEBSOCK_DATA &getDataForWebSocket();
bool readModbus();

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
bool readModbus()
{
    // read smart meter
    if (!mb_readSmartMeter(setupData, webSockData.mbContainer))
        return false;
    // read inverter
    if (!mb_readInverter(setupData, webSockData.mbContainer))
        return false;

    // read smart meter
    /* if (!mb_readInverterDynamic(setupData, webSockData.mbContainer))
        return false; */
    return true;
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
    eprom_getSetup(setupData);
    eprom_getLifeData(lifeData);
    // eprom_show(setupData);

    // eprom_test_read_Eprom();

    // if (strcmp(setupData.ssid, "---") == 0)
    if (strcmp(setupData.ssid, "--") == 0)
    {
        networkCredentialsInEEprom = false;
        webSockData.states.networkOK = false;
        www_init(NULL, NULL, getDataForWebSocket); // act as access point
    }
    if (networkCredentialsInEEprom)
    {

        char buff[130];
        memset(buff, 0, strlen(buff));

        if (!wifi_init(setupData))
        {
            DBGf("Cannot connect - show available networks: ");
            // tft_drawNetworkInfo(NULL, setupData.ssid);
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

            tft_drawNetworkInfo(globalStringBuffer, setupData.ssid);
            webSockData.states.flashOK = www_init(pBuf, setupData.ssid, getDataForWebSocket); // do not act as apoint
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
        DBGf("Setup Modbus ...");
        if (!mb_init(setupData))
        {
            DBGf("Cannot initialize modbus ....");
            tft_printKeyValue("Init mdobus", "Error", TFT_RED);
            webSockData.states.modbusOK = false;
        }
        else
        {
            webSockData.states.modbusOK = true;
            tft_printKeyValue("Init Modbus", "ok", TFT_GREEN);
        }
        memset(&webSockData.mbContainer, 0, sizeof(MB_CONTAINER));
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
        webSockData.states.cardWriterOK = false;
    }

    if (temp_init())
    { // temperature

        tft_printKeyValue("Init Sensors", "OK", TFT_GREEN);
        webSockData.states.tempSensorOK = true;
    }
    else
    {
        webSockData.states.tempSensorOK = false;
        tft_printKeyValue("Init Sensors", "Error", TFT_RED);
    }
    /* DBGf("Setup Modbus ...");
    if (!mb_init(setupData))
    {
        DBGf("Cannot initialize modbus ....");
        tft_printKeyValue("Init mdobus", "Error", TFT_RED);
        webSockData.states.modbusOK = false;
    }
    else
    {
        webSockData.states.modbusOK = true;
        tft_printKeyValue("Init Modbus", "ok", TFT_GREEN);
    }
    memset(&webSockData.mbContainer, 0, sizeof(MB_CONTAINER));
    */
    /*  if (!mb_readInverterStatic())
         DBGf("Error in Reading modbus"); */
    DBGf("Setup PID-Controller");
#ifdef MQTT
    mqtt_publish_pidParams(setupData.pid_p, setupData.pid_i, setupData.pid_d);
#endif
    DBGf("Mqtt - PID params:  p: %.2lf  i: %.2lf    d: %.2lf", setupData.pid_p, setupData.pid_i, setupData.pid_d);
    tft_printKeyValue("Init PID-Manager", "ok", TFT_GREEN);
    pidPinManager.config(setupData, RELAY_L1, RELAY_L2, PWM_FOR_PID);
}
if (webSockData.states.networkOK)
{
    delay(10000);
    tft_clearScreen();
}
ESP_LOGI(TAG, "Setup done - all components are working...");
#else
        ESP_LOGI(TAG, "Start testing...");

        pinMode(SWITCH_KEY_ENTER, INPUT_PULLUP);
        pinMode(SWITCH_KEY_UP, INPUT_PULLUP);
        pinMode(SWITCH_KEY_DOWN, INPUT_PULLUP);
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

bool readModbus()
{
    // read inverter
    if (!mb_readSmartMeter(setupData, webSockData.mbContainer))
        return false;
    if (!mb_readInverter(setupData, webSockData.mbContainer))
        return false;

    // read smart meter
    /* if (!mb_readInverterDynamic(setupData, webSockData.mbContainer))
        return false; */
    return true;
}
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
        sprintf(formatBuffer, "%d", setupData.heizstab_leistung_in_watt);
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
                if (!readModbus())
                {
                    tft_print_test(11, 15, 150, TFT_RED, "Error", "Modbus cannot read");
                    DBGf("Error reading modbus");
                    return;
                }
                if (!readModbus())
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
                if (!readModbus())
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
                readModbus();

                double dTime = difftime(time2, time1);
                sprintf(formatBuffer, "%.1f secs", dTime - 1.0);
                doneStop = true;
                DBGf("Stop Messung within %f secs", dTime);
                sprintf(formatBuffer, "%0.1lf %+6.1f | %+6.1f | %+6.1f", dTime, INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower - currentConsumeInWatt, INVERTER_DATA.acCurrentPower, METER_DATA.acCurrentPower);

                tft_print_test(7, 15, 80, TFT_GREEN, "Diff (W) ", formatBuffer);
                DBGf("Stop - last values: %s", formatBuffer);
                sprintf(formatBuffer, "Time: %.1f", dTime);
                tft_print_test(8, 15, 130, TFT_GREEN, "Stop (secs) ", formatBuffer);
                readModbus();
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
        // time_print();
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
                    /*   pinMode(RELAY_L1, OUTPUT);
                      pinMode(RELAY_L2, OUTPUT);
                      pinMode(PWM_FOR_PID, OUTPUT);
                      digitalWrite(RELAY_L1, 0);
                      digitalWrite(RELAY_L2, 0);
                      analogWrite(PWM_FOR_PID, 0); */

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
            // memcpy(&webSockData.temperature, &container, sizeof(TEMPERATURE));
            /*
            RELAY_L1, RELAY_L2, PWM_FOR_PID
            */
            DBGf(" TEMP in Celsius, S1: %f, S2: %f", webSockData.temperature.sensor1, webSockData.temperature.sensor2);
            if (!alarmContainer.alarmTemp.alarmTemp)
            {
                if (((int)(webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2.0) > setupData.tempMaxAllowedInGrad)
                {
                    ESP_LOGE(TAG, "Temperaturlimit erreicht - Heizpatrone wird abgeschaltet");
                    /*   pinMode(RELAY_L1, OUTPUT);
                     pinMode(RELAY_L2, OUTPUT);
                     pinMode(PWM_FOR_PID, OUTPUT);
                     digitalWrite(RELAY_L1, 0);
                     digitalWrite(RELAY_L2, 0);
                     analogWrite(PWM_FOR_PID, 0); */
                    pidPinManager.reset(); // alles aus
                    alarmContainer.alarmTemp.alarmTemp = true;
                    alarmContainer.alarmTemp.overFlowHappenedAt = time_getTimeStamp();
                    webSockData.temperature.alarm = true;
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
                    DBGf("TempLimit over %d °C , wait for next check in secs: %d", setupData.tempMaxAllowedInGrad, TEMPERATURE_OVERHEATED_WAIT_IN_SECS);
                    if (((int)(webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2.0) > setupData.tempMaxAllowedInGrad)
                    {
                        alarmContainer.alarmTemp.overFlowHappenedAt = time_getTimeStamp();
#ifdef MQTT
                        mqtt_publish_alarm_temp(webSockData.temperature.sensor1, webSockData.temperature.sensor2);
#endif
                    }
                    else
                    {
                        alarmContainer.alarmTemp.alarmTemp = true;
                        alarmContainer.alarmTemp.overFlowHappenedAt = 0;
                        DBGf("TempLimit reset");
                    }
                }
            }
        } // !getTemperature()
        timeSlice.previousMillTemp = timeSlice.currentMillis;
    }

    /* ***********************                   MODBUS           ************************/
    if (timeSlice.currentMillis - timeSlice.previousMillModbus > MODBUS_INTERVALL)
    {
        if (!webSockData.states.modbusOK)
        {
            tft_drawInfoNoModbus(webSockData.temperature);
            if (mb_init(setupData))
            {
                DBGf("Reconnected modbus successfully.");
                webSockData.states.modbusOK = true;
#ifdef MQTT
                mqtt_publish_modbus_reconnect(setupData.ipInverterAsString.c_str());
#endif
            }
        }
        else
        {

            if (readModbus())
            {

                memset(formatBuffer, 0, FORMAT_CHAR_BUFFER_LEN);
                // memcpy(&webSockData.mbContainer, &modbusData, sizeof(MB_CONTAINER));
                util_format_Watt_kWatt(INVERTER_DATA.acCurrentPower, formatBuffer);
                DBGf("Produktion %s", formatBuffer);

                DBGf("EXport %s", util_format_Watt_kWatt(METER_DATA.acCurrentPower, formatBuffer));

                if (METER_DATA.acCurrentPower < 0.0 && (INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower < 0))

                {
                    DBGf("Wrong meter value from smartmeter - current production: %f !", METER_DATA.acCurrentPower);
#ifdef MQTT
                    mqtt_publish_modbus_wrong_production_val(METER_DATA.acCurrentPower);
#endif
                }
                else
                {
                    // memset(&pidContainer, 0, sizeof(pidContaienr));

                    DBGf("Verbrauch in W: %s", util_format_Watt_kWatt(INVERTER_DATA.acCurrentPower + METER_DATA.acCurrentPower, formatBuffer));
                    webSockData.pidContainer.mCurrentPower = METER_DATA.acCurrentPower; // export energy
                    // DBGf(", int: %d", pidContainer.mCurrentPower);
                    // pidContainer.mCurrentPower = (int)pidPinManager.getCurrentPower();

                    webSockData.pidContainer.powerNotUseable = (int)pidPinManager.getReservedPower() + 0.5; // ????
                    webSockData.pidContainer.mAnalogOut = pidPinManager.getStateOfAnaPin();
                    webSockData.pidContainer.PID_PIN1 = pidPinManager.getStateOfDigPin(0); // PIN 1
                    webSockData.pidContainer.PID_PIN2 = pidPinManager.getStateOfDigPin(1); // PIN 2
#ifndef TEST_PID_WWWW
                    availablePowerFromWRInWatt = webSockData.pidContainer.mCurrentPower;
#endif

                    tft_drawInfo(webSockData.temperature, webSockData.mbContainer, webSockData.pidContainer);
#ifdef MQTT
                }
                mqtt_publish_modbus_current_state(webSockData.mbContainer);
#endif
            }
            else
            { // if readModbus
                DBGf("MAIN::ModbusTimeSlice::  Error in reading modubs");
            }
        } // if modbusstate
        timeSlice.previousMillModbus = timeSlice.currentMillis;
    } // if

    /* ***********************                   FLUSH LOGGING FILE           ************************/

    if (timeSlice.currentMillis - timeSlice.previousMillModbus > LOGGING_FLUSH_INTERVALL)
    {
        cardRW_flushLoggingFile();
        timeSlice.previousMillModbus = timeSlice.currentMillis;
        cardRW_closeLoggingFile();
    }
    /* ***********************                   PID CONTROLLER           ************************/
    if (timeSlice.currentMillis - timeSlice.previousMillisController > PID_CONTROLLER_INTERVALL)
    {

        if (!alarmContainer.alarmTemp.alarmTemp)
        {

            if (availablePowerFromWRInWatt < 0.0) // energy export
            {
                // DBGf(" main::Einspeisung %lf, muss übrig bleiben %d", availablePowerFromWRInWatt, setupData.pid_powerWhichNeedNotConsumed);

                //  Einspeisung - Wieviel müss übrig bleiben
                pidPinManager.task(setupData, &availablePowerFromWRInWatt);
            }
            else
            {

                // DBGf(" main::Bezug  %f", availablePowerFromWRInWatt);

                pidPinManager.task(setupData, &availablePowerFromWRInWatt);
            }
            timeSlice.previousMillisController = timeSlice.currentMillis;
        }
        // delay(2000);
        // DBGf(" main:: available watt: %f", availablePowerFromWRInWatt);

        /* ***********************                   CONFIG LIVE UPDATE sTAMMdaTEN via WEB           ************************/
        if (timeSlice.currentMillis - timeSlice.previousMillisTestConfig > CONFIG_PARAM_TEST_INTERVALL)
        {
            timeSlice.previousMillisTestConfig = timeSlice.currentMillis;
#ifdef TEST_PID_WWWW
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

            setupData.pid_p = d.pid_p;
            setupData.pid_d = d.pid_d;
            setupData.pid_i = d.pid_i;
            mqtt_publish_pidParams(setupData.pid_p, setupData.pid_i, setupData.pid_d);

#endif
        }

        if (timeSlice.currentMillis - timeSlice.previousMillisWebSocks > WEBSOCK_NOTIFY_INTERVALL)
        {
            timeSlice.previousMillisWebSocks = timeSlice.currentMillis;
            notifyClients(getJsonObj());
        }
        if (networkCredentialsInEEprom == false)
        { // act as AP
            www_run();
        }

#endif
    }
} // loop

WEBSOCK_DATA &getDataForWebSocket()
{
    // DBGf("getDataForWebSocket, Temp: %.2lf", webSockData.temperature.sensor1);
    return webSockData;
}