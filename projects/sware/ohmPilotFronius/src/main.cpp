#include <Arduino.h>
#include "esp_clk.h"
#include <SPI.h>
#include "debugConsole.h"
#include "wlan.h"
#include "modbusReader.h"
#include "cardRW.h"
#include "utils.h"
#include "tft.h"
// #include "graphicTest.h"
#include "eprom.h"
#include "pidRegler.h"

#include "ap.h"
#include "temp.h"
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

/* ****************************************************************************
  GLOBAL VARS
  ****************************************************************************
*/

char globalStringBuffer[GLOBAL_STRING_BUFFER_LEN];
static bool networkCredentialsInEEprom = true;
static bool cardWriterOK = false;

static const char *wlanE = "Milchbehaelter";
static const char *passW = "47754775";
static unsigned long previousMillTemp = 0UL;
static unsigned long previousMillModbus = 0UL;
static TEMPERATURE container;

void test()
{
  StaticJsonDocument<100> data;
  const char *argument = "234";
  bool errorH = util_isFieldFilled("123", argument, data);
  int result = 1;
  errorH = util_checkParamInt(HEIZPATRONE, argument, data, &result);
  int r = 0;
  r = atoi(argument);
  DBGln(r);
}

void test_cardReader()
{
  cardRW_listDir("/", 3);
}

void setup()
{

  Serial.begin(115200);
  while (!Serial)
    ;
  DBGln("Energie-Junkies -- Harvester ---");
  // test();
  /*  tft_init();
   tft_printSetup(); */
  int currentState = digitalRead(INTERNAL_BUTTON_2_GPIO);
  DBG("internal bu: ");
  DBGln(currentState);
  uint32_t cpu_freq = esp_clk_cpu_freq();
  DBG(" CPU freq: ");
  DBGln(cpu_freq);
  uint32_t PRESCALE = 240; // for 240MHZ
  Setup setup;             // all input quantities
  // eprom_test_write_Eprom(wlanE, passW);
  eprom_getSetup(setup);
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

  // eprom_test_read_Eprom();
  if (cardRW_setup())
  {
    cardWriterOK = true;
    test_cardReader();
  }

  temp_init(); // temperature

  if (strcmp(setup.ssid, "---") == 0)
  {
    networkCredentialsInEEprom = false;
    ap_init(); // act as access point
  }
  if (networkCredentialsInEEprom)
  {

    WiFi.mode(WIFI_STA);

    WiFi.begin(setup.ssid, setup.passwd);
    DBG("Connecting to WiFi ..");
    int counter = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
      DBG('.');
      delay(1000);
      ++counter;
      if (counter == 5)
        break;
    }
    if (!wifi_init(setup))
    {
      DBGln("Cannot connect - show available networks: ");
      tft_drawNetworkInfo(NULL);
      wifi_scan_network();
      ap_init();
    }
    else
    {
      DBG("Connected with ip: ");
      char *pBuf = globalStringBuffer;
      wifi_getLocalIP(&pBuf);
      DBGln(globalStringBuffer);
      tft_drawNetworkInfo(globalStringBuffer);
    }

    DBGln("Setup modbus ...");
    if (!mb_init(setup))
    {
      DBGln("Cannot initialize modbus ....");
    }

    /* if (!pid_init(&setup))
    {
      DBGln("Cannot initialize pid controller:");
    }
    else
    {
      DBGln("PID controller initializred successfully ....");
    } */
  }
}

void loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillTemp > TEMPERATURE_INTERVAL)
  {
    DBG(" TEMP in Celsius");
    temp_getTemperature(container);
    DBG("Sensor1: ");
    DBG(container.sensor1);
    DBG(", Sensor 2: ");
    DBGln(container.sensor2);
    previousMillTemp = currentMillis;
  }
  /* if (networkCredentialsInEEprom == false)
    return; */
  if (currentMillis - previousMillModbus > MODBUS_INTERVALL)

  {

    if (!mb_readInverterStatic())
      DBGln("Error in Reading modbus");
    else
      DBGln(" --- MODBUS --- Query done");
    previousMillModbus = currentMillis;
    // pid_run(400.0);
  }
  delay(4000);

  if (!cardWriterOK)
    cardWriterOK = cardRW_setup();
  if (networkCredentialsInEEprom == false)
  { // act as AP
    ap_run();
  }
  else
  {
    /*
    mb_readInverter();
    */
    // tft_printTxt(30, 50, 2, "test");
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
