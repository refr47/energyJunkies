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

/*
Input only pins
GPIOs 34 to 39 are GPIs – input only pins. These pins don’t have internal pull-up or pull-down resistors. They can’t be used as outputs, so use these pins only as inputs:

https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
*/

/* ****************************************************************************

  DEFInES

  ****************************************************************************
 */

#define GLOBAL_STRING_BUFFER_LEN 150
#define INTERNAL_BUTTON_1_GPIO 0
#define INTERNAL_BUTTON_2_GPIO 14

#define TEMPERATURE_INTERVAL 5000UL // 5 secs
#define MODBUS_INTERVALL 5000UL
#define LOGGING_FLUSH_INTERVALL 60000

#ifndef TAG
#define TAG "E-JUNKIES"
#endif
#define LOG_LEVEL ESP_LOG_INFO
#define MY_ESP_LOG_LEVEL ESP_LOG_INFO

/* ****************************************************************************
  GLOBAL VARS
  ****************************************************************************
*/

char globalStringBuffer[GLOBAL_STRING_BUFFER_LEN];
static bool networkCredentialsInEEprom = true;
static bool cardWriterOK = false;
static bool networkOK = false;

static const char *wlanE = "Milchbehaelter";
static const char *passW = "47754775";
static unsigned long previousMillTemp = 0UL;
static unsigned long previousMillModbus = 0UL;
static unsigned long previousMillFlush = 0UL;
static TEMPERATURE container;
static MB_CONTAINER modbusData;
static PID_CONTAINER pidContainer;
static Setup setupData;
static PinManager pidPinManager(RELAY_L1, RELAY_L2, PWM_FOR_PID);

/* **************************************************************************
        ProtoTypes
*/
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

void test()
{
    StaticJsonDocument<JSON_OBJECT_SETUP_LEN> data;
    const char *argument = "234";
    bool errorH = util_isFieldFilled("123", argument, data);
    int result = 1;
    errorH = util_checkParamInt(IP_INVERTER, argument, data, &result);
    int r = 0;
    r = atoi(argument);
    DBGf("RetVal: %d", r);
}

void test_cardReader()
{
    cardRW_listDir("/", 3);
}

void setup()
{

    DBGbgn(115200);
    while (!Serial)
        ;

    DBGf("Energie-Junkies -- Harvester ---");
    // test();
    tft_init();

    tft_printSetup();

    /*  int currentState = digitalRead(INTERNAL_BUTTON_2_GPIO);
     DBGf("Interal button: %d", currentState); */

    /* uint32_t cpu_freq = esp_clk_cpu_freq();
    DBG(" CPU freq: ");
    DBGln(cpu_freq);
    uint32_t PRESCALE = 240; // for 240MHZ */

    // eprom_test_write_Eprom(wlanE, passW);
    eprom_getSetup(setupData);
    eprom_test_read_Eprom();

    /*
     printHWInfo();
     */
    /*enable internal buttons*/
    /* pinMode(INTERNAL_BUTTON_1_GPIO, INPUT_PULLUP);
    pinMode(INTERNAL_BUTTON_2_GPIO, INPUT_PULLUP);
    pinMode(PIN, OUTPUT); */
    // delay(500);
    // pHW();
    //  digitalWrite(PIN, 0);
    //  wifi_scan_network();
    // tft_clearScreen();
    // meterSim();
    // eprom_test_write_Eprom(wlanE, passW);
    /* DBGln(">>>>>>>>>>>eprom test");
     */
    /*  eprom_test_write_Eprom(wlanE, passW);
     eprom_test_read_Eprom();
     DBGln(">>>>>>>>>>>eprom test end");
     delay(1000); */
    // wifi_scan_network();
    //  eprom_test_read_Eprom();

    if (strcmp(setupData.ssid, "---") == 0)
    {
        networkCredentialsInEEprom = false;
        www_init(NULL, NULL); // act as access point
    }
    if (networkCredentialsInEEprom)
    {

        WiFi.mode(WIFI_STA);

        WiFi.begin(setupData.ssid, setupData.passwd);
        DBGf("WIFI: %s, Passwd: %s", setupData.ssid, setupData.passwd);
        DBGf("Connecting to WiFi ..");
        tft_printInfo("Connecting to WiFi");
        int counter = 0;
        char buff[30];
        memset(buff, 0, strlen(buff));
        while (WiFi.status() != WL_CONNECTED)
        {
            DBG("%c", '.');
            delay(2000);
            ++counter;
            for (int kk = 0; kk < counter; kk++)
            {
                buff[kk] = '.';
            }
            tft_printInfo(buff, false);
            if (counter == 5)
            {
                tft_printInfo("", true);
                tft_printInfo("Scanning WiFi ..");
                break;
            }
        }
        if (!wifi_init(setupData))
        {
            DBGf("Cannot connect - show available networks: ");
            // tft_drawNetworkInfo(NULL, setupData.ssid);
            wifi_scan_network();
            www_init(NULL, NULL); // act as access point
            networkOK = false;
        }
        else
        {

            char *pBuf = globalStringBuffer;
            wifi_getLocalIP(&pBuf);
            DBGf("Connected with ip: %s", globalStringBuffer);

            tft_drawNetworkInfo(globalStringBuffer, setupData.ssid);
            www_init(pBuf, setupData.ssid); // do not act as apoint
            networkOK = true;
        }
        if (!networkOK)
        {
            DBGf("Network does not work!");
            tft_printInfo("No valid network!");
            return;
        }
        tft_printInfo("       ");
        tft_printKeyValue("Init Time", "OK", DONE_);
        time_init(); // init time

        if (cardRW_setup(false, false))
        {
            cardWriterOK = true;
            tft_printKeyValue("Init CardReader", "OK", DONE_);

            logging_init();
            // test_cardReader();
        }
        else
        {
            DBGf("Logging to file cannot be initiated ...");
            tft_printKeyValue("Init CardReader", "Error", ALARM_);
        }

        if (temp_init())
        { // temperature

            tft_printKeyValue("Init Sensors", "OK", DONE_);
        }
        else
        {

            tft_printKeyValue("Init Sensors", "Error", ALARM_);
        }
        DBGf("Setup modbus ...");
        tft_printInfo("Init Modbus ");
        if (!mb_init(setupData))
        {
            DBGf("Cannot initialize modbus ....");
            tft_printKeyValue("Init mdobus", "Error", ALARM_);
        }
        else
        {

            tft_printKeyValue("Init mdobus", "ok", DONE_);
        }
        memset(&modbusData, 0, sizeof(modbusData));
        /*  if (!mb_readInverterStatic())
             DBGf("Error in Reading modbus"); */
        DBGf("Setup PID-Controller");
        tft_printKeyValue("Init PID-Manager", "ok", DONE_);
        pidPinManager.config(setupData);
    }
    if (networkOK)
    {
        delay(3000);
        tft_clearScreen();
    }
    ESP_LOGI(TAG, "Setup done - all components are working...");
}

static unsigned long currentMillis = millis();
static char formatBuffer[25];

void loop()
{
    if (!networkOK)
    {
        DBGf("Network does not work - no further task are available...");
        delay(10000);
        return;
    }
    currentMillis = millis();
    if (currentMillis - previousMillTemp > TEMPERATURE_INTERVAL)
    {
        time_print();
        temp_getTemperature(container);

        DBGf(" TEMP in Celsius, S1: %f, S2: %f", container.sensor1, container.sensor2);

        previousMillTemp = currentMillis;
    }
    /* if (networkCredentialsInEEprom == false)
      return; */
    if (currentMillis - previousMillModbus > MODBUS_INTERVALL)
    {
        mb_readInverterDynamic(setupData, modbusData);
       memset(formatBuffer,0,25);
        util_format_Watt_kWatt(modbusData.inverterSumValues.data.acCurrentPower,formatBuffer);
        DBGf("Produktion %s", formatBuffer);

        DBGf("EXport %s", util_format_Watt_kWatt(modbusData.meterValues.data.acCurrentPower,formatBuffer) );

        if (modbusData.meterValues.data.acCurrentPower<0.0 &&  (modbusData.inverterSumValues.data.acCurrentPower  + modbusData.meterValues.data.acCurrentPower < 0))

        {
            DBGf("Wrong meter value !");
        }
        else
        {
           DBGf("Verbrauch in W: %s",util_format_Watt_kWatt( modbusData.inverterSumValues.data.acCurrentPower + modbusData.meterValues.data.acCurrentPower,formatBuffer));
            pidContainer.mCurrentPower = (int)modbusData.meterValues.data.acCurrentPower + 0.5; // export!
            //DBGf(", int: %d", pidContainer.mCurrentPower);

            if (pidContainer.mCurrentPower < 0) // energy export
            {
                pidContainer.mCurrentPower = pidContainer.mCurrentPower * -1;
                pidPinManager.task(setupData, pidContainer);
            }
            else
            {
                pidContainer.mCurrentPower = 4;
                pidPinManager.task(setupData, pidContainer);
            }
            tft_drawInfo(container, modbusData, pidContainer);
        }
        previousMillModbus = currentMillis;
    }
    if (currentMillis - previousMillModbus > LOGGING_FLUSH_INTERVALL)
    {
        cardRW_flushLoggingFile();
        previousMillModbus = currentMillis;
        cardRW_closeLoggingFile();
    }
    delay(4000);

    if (networkCredentialsInEEprom == false)
    { // act as AP
        www_run();
    }
    else
    {
        /*
        mb_readInverter();
        */
       
        //  cardRW_setup();
        //  test_cardReader();
        //  temp_init();

        // delay(1000);
        /* if (!mb_readInverter())
        {
          DBGln("Cannot read Inverter ...");
        } */
        /* DBGln(" .... LOOP .....");
        int currentState = digitalRead(INTERNAL_BUTTON_2_GPIO);
        DBG("internal bu: ");
        DBGln(currentState); */
    }
}
