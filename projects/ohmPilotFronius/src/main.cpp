#include <Arduino.h>

#include <TFT_eSPI.h>
#include <SPI.h>
#include "wlan.h"
#include "modbusReader.h"
#include "cardRW.h"
#include "img/wlanPic24.h"

/*   DEFInES
 */

#define GLOBAL_STRING_BUFFER_LEN 150
#define INTERNAL_BUTTON_1_GPIO 0
#define INTERNAL_BUTTON_2_GPIO 35

/* ***********************************
  GLOBAL VARS
*/

TFT_eSPI tft = TFT_eSPI();
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

static void printHWInfo()
{
  Serial.print("MEM: ");
  Serial.println(esp_get_free_heap_size());

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  Serial.println("Hardware info");
  Serial.printf("%d cores Wifi %s%s\n", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
  Serial.printf("Silicon revision: %d\n", chip_info.revision);

  // get chip id
  uint32_t chipId = ESP.getEfuseMac();

  Serial.printf("Chip id: %x\n", chipId);
}

static void drawNetworkInfo(char *ip)
{
  if (ip == NULL)
  {
    tft.pushImage(0, 0, 24, 24, wlanPic24);
    delay(500);                            // 500ms warten
    tft.fillRect(0, 0, 24, 24, TFT_BLACK); // Bild ausschalten
    delay(500);                            // 500ms warten
  }
  else
  {
    tft.pushImage(0, 0, 24, 24, wlanPic24);
    tft.setCursor(30, 0, 4);
    tft.println(ip);
  }
}

void setup()
{
  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  Serial.begin(115200);
  delay(500);
  // while (!Serial) ;
  /*
    Serial.print("MOSI: ");
    Serial.println(MOSI);
    Serial.print("MISO: ");
    Serial.println(MISO);
    Serial.print("SCK: ");
    Serial.println(SCK);
    Serial.print("SS: ");
    Serial.println(SS); */

  printHWInfo();
  /*enable internal buttons*/
  pinMode(INTERNAL_BUTTON_1_GPIO, INPUT_PULLUP);
  pinMode(INTERNAL_BUTTON_2_GPIO, INPUT_PULLUP);
  Serial.println("Setup card reader ...");
  if (!cardRW_setup())
  {
    Serial.println("Cannot setup card reader ....");
  }
  else
  {
    Serial.println("Card_RW: has initialized");
  }
  Serial.println("Setup wlan ...");
  tft.setCursor(0, 0, 4);

  if (!wifi_init())
  {
    Serial.println("Cannot connect");
    drawNetworkInfo(NULL);
  }
  else
  {
    Serial.print("Connected with ip: ");
    char *pBuf = globalStringBuffer;
    wifi_getLocalIP(&pBuf);
    Serial.println(globalStringBuffer);
    drawNetworkInfo(globalStringBuffer);
  }
}

void loop()
{
  // put your main code here, to run repeatedly:

  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.setCursor(0, 80, 4);

  tft.println("OK");

  delay(200);
}
