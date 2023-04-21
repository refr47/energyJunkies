#include <Arduino.h>

#include <TFT_eSPI.h>
#include "wlan.h"
#include "modbusReader.h"

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

void setup()
{
  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  Serial.begin(115200);
  delay(400);

  /*enable internal buttons*/
  pinMode(INTERNAL_BUTTON_1_GPIO, INPUT_PULLUP);
  pinMode(INTERNAL_BUTTON_2_GPIO, INPUT_PULLUP);
  tft.setCursor(0, 0, 4);
  if (!wifi_init())
    tft.println("Cannot connect");
  else
    tft.println("Connected");
  tft.setCursor(0, 30, 4);
  char *pBuf = globalStringBuffer;
  wifi_getLocalIP(&pBuf);
  tft.println(globalStringBuffer);
}

void loop()
{
  // put your main code here, to run repeatedly:

  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.setCursor(0, 80, 4);

  tft.println("");

  delay(200);
}
