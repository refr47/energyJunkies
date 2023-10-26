#include <Arduino.h>

#include <SPI.h>
#include "wlan.h"
#include "modbusReader.h"
#include "cardRW.h"
#include "utils.h"
#include "tft.h"
// #include "graphicTest.h"
#include "eprom.h"

#include "ap.h"
#include "temp.h"
/*
Input only pins
GPIOs 34 to 39 are GPIs – input only pins. These pins don’t have internal pull-up or pull-down resistors. They can’t be used as outputs, so use these pins only as inputs:

GPIO 34
GPIO 35
GPIO 36
GPIO 39
https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
*/
/*
Many ESP32 boards come with default SPI pins pre-assigned. The pin mapping for most boards is as follows:

SPI	MOSI	MISO	SCLK	CS
VSPI	GPIO 23	GPIO 19	GPIO 18	GPIO 5
HSPI	GPIO 13	GPIO 12	GPIO 14	GPIO 15


*/

/*   DEFInES
 */

#define GLOBAL_STRING_BUFFER_LEN 150
#define INTERNAL_BUTTON_1_GPIO 0
#define INTERNAL_BUTTON_2_GPIO 14

#define PIN 21

/* ***********************************
  GLOBAL VARS
*/

char globalStringBuffer[GLOBAL_STRING_BUFFER_LEN];
static bool networkCredentialsInEEprom = true;

static const char *wlanE = "Milchbehaelter";
static const char *passW = "47754775";

void test()
{
  StaticJsonDocument<100> data;
  const char *argument = "234";
  bool errorH = util_isFieldFilled("123", argument, data);
  int result = 1;
  errorH = util_checkParamInt(HEIZPATRONE, argument, data, &result);
  int r = 0;
  r = atoi(argument);
  Serial.println(r);
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
  Serial.println("Energie-Junkies -- Harvester ---");
  // test();
  /*  tft_init();
   tft_printSetup(); */
  int currentState = digitalRead(INTERNAL_BUTTON_2_GPIO);
  Serial.print("internal bu: ");
  Serial.println(currentState);

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
  /* Serial.println(">>>>>>>>>>>eprom test");
  eprom_test_write_Eprom(wlanE, passW);
  eprom_test_read_Eprom();
  Serial.println(">>>>>>>>>>>eprom test end");
  delay(1000); */

  // eprom_test_read_Eprom();
  // cardRW_setup();
  // test_cardReader();
  temp_init();
  Setup setup;
  eprom_getSetup(setup);

  if (strcmp(setup.ssid, "---") == 0)
  {
    networkCredentialsInEEprom = false;
    ap_init(); // act as access point
  }
  if (networkCredentialsInEEprom)
  {
    WiFi.mode(WIFI_STA);
    // WiFi.begin(setup.ssid, setup.passwd);
    WiFi.begin(setup.ssid, setup.passwd);
    Serial.print("Connecting to WiFi ..");
    int counter = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print('.');
      delay(1000);
      ++counter;
      if (counter == 10)
        break;
    }
    if (!wifi_init())
    {
      Serial.println("Cannot connect - show available networks: ");
      tft_drawNetworkInfo(NULL);
      wifi_scan_network();
      ap_init();
    }
    else
    {
      Serial.print("Connected with ip: ");
      char *pBuf = globalStringBuffer;
      wifi_getLocalIP(&pBuf);
      Serial.println(globalStringBuffer);
      tft_drawNetworkInfo(globalStringBuffer);
    }

    Serial.println("Setup card reader ...");
    /* if (!cardRW_setup())
    {
      Serial.println("Cannot setup card reader ....");
    }
    else
    {
      Serial.println("Card_RW: has initialized");
    }
    Serial.println("Setup modbus ...");
    if (!mb_init())
    {
      Serial.println("Cannot initialize modbus ....");
    }
    else
    {
      mb_readInverter();
    } */
  }
}

void loop()
{
  if (networkCredentialsInEEprom == false)
  { // act as AP
    ap_run();
  }
  else
  {
    /*
    mb_readInverter();
    */
    tft_printTxt(30, 50, 2, "test");
    // cardRW_setup();
    // test_cardReader();
    // temp_init();
    Serial.println(" TEMP :::::::::::");
    Serial.print(getTempSensor1());

    delay(1000);
    /* if (!mb_readInverter())
    {
      Serial.println("Cannot read Inverter ...");
    } */
    Serial.println(" .... LOOP .....");
    int currentState = digitalRead(INTERNAL_BUTTON_2_GPIO);
    Serial.print("internal bu: ");
    Serial.println(currentState);
    delay(5000);
  }
}
