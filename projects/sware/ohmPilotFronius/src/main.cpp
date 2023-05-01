#include <Arduino.h>

#include <SPI.h>
#include "wlan.h"
#include "modbusReader.h"
#include "cardRW.h"

#include "tft.h"

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
#define INTERNAL_BUTTON_2_GPIO 35

#define PIN 21

/* ***********************************
  GLOBAL VARS
*/

char globalStringBuffer[GLOBAL_STRING_BUFFER_LEN];
/*
static void testSPI()
{

  spi_device_handle_t spi;

  spi_bus_config_t buscfg = {
      .miso_io_num = -1,
      .mosi_io_num = -1,
      .sclk_io_num = -1,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
  };

  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 1000000,
      .mode = 0,
      .spics_io_num = -1,
      .queue_size = 1,
  };



  for (int i = 0; i < 40; i++)
  {
    buscfg.miso_io_num = i;
    buscfg.mosi_io_num = i;
    buscfg.sclk_io_num = i;
    buscfg.spics_io_num = i;

    esp_err_t ret = spi_bus_add_device(SPI2_HOST, &buscfg, &devcfg, &spi);
    if (ret == ESP_OK)
    {
      printf("GPIO %d supports SPI communication.\n", i);
      spi_bus_remove_device(spi);
    }
  }
}
*/

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  tft_init();
  /*
  Serial.print("MOSI: ");
  Serial.println(MOSI);
  Serial.print("MISO: ");
  Serial.println(MISO);
  Serial.print("SCK: ");
  Serial.println(SCK);
  Serial.print("SS: ");
  Serial.println(SS);

  printHWInfo();
  */
  /*enable internal buttons*/
  pinMode(INTERNAL_BUTTON_1_GPIO, INPUT_PULLUP);
  pinMode(INTERNAL_BUTTON_2_GPIO, INPUT_PULLUP);
  pinMode(PIN, OUTPUT);
  delay(500);
  digitalWrite(PIN, 0);
  Serial.println("Setup card reader ...");
  /*  if (!cardRW_setup())
   {
     Serial.println("Cannot setup card reader ....");
   }
   else
   {
     Serial.println("Card_RW: has initialized");
   } */
  Serial.println("Setup wlan ...");

  // tft.setCursor(0, 0, 4);
  wifi_scan_network();
  /*
    if (!wifi_init())
    {
      Serial.println("Cannot connect");
      tft_drawNetworkInfo(NULL);
    }
    else
    {
      Serial.print("Connected with ip: ");
      char *pBuf = globalStringBuffer;
      wifi_getLocalIP(&pBuf);
      Serial.println(globalStringBuffer);
      tft_drawNetworkInfo(globalStringBuffer);
    } */
}

void loop()
{
  // put your main code here, to run repeatedly:
  // setCursor(0, 30);
  // printTxt(0, 30, 4, "JA");
  // wifi_scan_network();
  digitalWrite(PIN, 0);

  delay(500);
}
