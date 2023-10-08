#include <Arduino.h>

#include <SPI.h>
#include "wlan.h"
#include "modbusReader.h"
#include "cardRW.h"

#include "tft.h"
// #include "graphicTest.h"
#include "eprom.h"

#include "ap.h"
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
static bool networkCredentialsInEEprom = false;

void pHW()
{
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  Serial.println("Hardware info");
  Serial.printf("%d cores Wifi %s%s\n", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
  Serial.printf("Silicon revision: %d\n", chip_info.revision);
  Serial.printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
                (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embeded" : "external");

  // get chip id
  String chipId = String((uint32_t)ESP.getEfuseMac(), HEX);
  chipId.toUpperCase();

  Serial.printf("Chip id: %s\n", chipId.c_str());
  Serial.print("Model: ");
  Serial.println(chip_info.model);
}

void setup()
{
  /* pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH); */

  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("Hello T-Display-S3");
  tft_init();
  tft_printSetup();
  int currentState = digitalRead(INTERNAL_BUTTON_2_GPIO);
  Serial.print("internal bu: ");
  Serial.println(currentState);
  /* Serial.print("MOSI: ");
  Serial.println(MOSI);
  Serial.print("MISO: ");
  Serial.println(MISO);
  Serial.print("SCK: ");
  Serial.println(SCK);
  Serial.print("SS: ");
  Serial.println(SS); */

  /*
   printHWInfo();
   */
  /*enable internal buttons*/
  /* pinMode(INTERNAL_BUTTON_1_GPIO, INPUT_PULLUP);
  pinMode(INTERNAL_BUTTON_2_GPIO, INPUT_PULLUP);
  pinMode(PIN, OUTPUT); */
  delay(500);
  pHW();
  // digitalWrite(PIN, 0);
  // wifi_scan_network();
  tft_clearScreen();
  // meterSim();
  cardRW_setup();
  Network n;
  eprom_getNetwork(n);

  if (n.ssid == "")
  {
    networkCredentialsInEEprom = false;
    ap_init(); // act as access point
  }
  if (networkCredentialsInEEprom)
  {
    if (!wifi_init())
    {
      Serial.println("Cannot connect");
      tft_drawNetworkInfo(NULL);
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
    cardRW_setup();
    delay(1000);
    /* if (!mb_readInverter())
    {
      Serial.println("Cannot read Inverter ...");
    } */
    Serial.println(" .... LOOP .....");
    int currentState = digitalRead(INTERNAL_BUTTON_2_GPIO);
    Serial.print("internal bu: ");
    Serial.println(currentState);
    delay(500);
  }
}
