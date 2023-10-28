#include <Arduino.h>
#include "esp_clk.h"
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

/*
  TIMER

hw_timer_t *timer5s = NULL;

void IRAM_ATTR onTimer5Sec();

void configTimer()
{
  Serial.print("Config timer");
  timer5s = timerBegin(0, 240, true);
  timerAttachInterrupt(timer5s, &onTimer5Sec, true);
  timerAlarmWrite(timer5s, 1000000, true);
  timerAlarmEnable(timer5s); // Just Enable
  Serial.println(" ...  Done");
}
*/

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
  uint32_t cpu_freq = esp_clk_cpu_freq();
  Serial.print(" CPU freq: ");
  Serial.println(cpu_freq);
  uint32_t PRESCALE = 240; // for 240MHZ
  Setup setup;             // all input quantities

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
  /* Serial.println(">>>>>>>>>>>eprom test");
   */
  /* eprom_test_write_Eprom(wlanE, passW);
  eprom_test_read_Eprom();
  Serial.println(">>>>>>>>>>>eprom test end");
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
    Serial.print("Connecting to WiFi ..");
    int counter = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print('.');
      delay(1000);
      ++counter;
      if (counter == 5)
        break;
    }
    if (!wifi_init(setup))
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

    Serial.println("Setup modbus ...");
    if (!mb_init(setup))
    {
      Serial.println("Cannot initialize modbus ....");
    }
    else
    {
      mb_readInverter();
    }
  }
  // configTimer();
}

/* void IRAM_ATTR onTimer5Sec()
{
  Serial.println(" 5 sec timer");
   if (networkCredentialsInEEprom == false)
    return;
  Serial.print(" TEMP in Celsius");
  temp_getTemperature(container);
  Serial.print("Sensor1: ");
  Serial.print(container.sensor1);
  Serial.print(", Sensor 2: ");
  Serial.println(container.sensor2);
  Serial.println(" EXIT 5 sec timer ...");
} */

void loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillTemp > TEMPERATURE_INTERVAL)
  {
    Serial.print(" TEMP in Celsius");
    temp_getTemperature(container);
    Serial.print("Sensor1: ");
    Serial.print(container.sensor1);
    Serial.print(", Sensor 2: ");
    Serial.println(container.sensor2);
    previousMillTemp = currentMillis;
  }
  /* if (networkCredentialsInEEprom == false)
    return; */
  if (currentMillis - previousMillModbus > MODBUS_INTERVALL)

  {

    if (!mb_readInverter())
      Serial.println("Error in Reading modbus");
    else
      Serial.println(" --- MODBUS --- Query done");
    previousMillModbus = currentMillis;
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
      Serial.println("Cannot read Inverter ...");
    } */
    /* Serial.println(" .... LOOP .....");
    int currentState = digitalRead(INTERNAL_BUTTON_2_GPIO);
    Serial.print("internal bu: ");
    Serial.println(currentState); */
  }
}
