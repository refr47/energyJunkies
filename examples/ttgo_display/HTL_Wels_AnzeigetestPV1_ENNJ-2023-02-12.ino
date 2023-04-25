

#include <TFT_eSPI.h>       // Hardware-specific library
#include "HTL.h"
TFT_eSPI tft = TFT_eSPI();  // Invoke custom library

int zahl = 0; 

void setup() {
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, 128, 112, HTL);

}

void loop() {
tft.setTextColor(TFT_BLUE, TFT_BLACK);
tft.setCursor(0, 160, 7);
tft.println(zahl);
zahl++;
delay(200);
if (zahl==1000){
  zahl=0;
}
//analogWrite(21, 128);
}
