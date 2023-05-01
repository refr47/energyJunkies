
#include <SPI.h>
#include "tft.h"
#include "img/wlanPic24.h"

/* GLOBAL VARS*/

static TFT_eSPI tft = TFT_eSPI(135, 240);
static int height = 0, width = 0;

void tft_init()
{
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);

    if (TFT_BL > 0)
    {                                           // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
        pinMode(TFT_BL, OUTPUT);                // Set backlight pin to output mode
        digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
    }

    tft.setSwapBytes(true);
    width = tft.width();
    height = tft.height();
    Serial.print("TFT Width,Height: ");
    Serial.print(width);
    Serial.println(height);
    tft.drawString("INIT", 10, 10);
}
TFT_eSPI &tft_getRoot()
{
    return tft;
}
int tft_getHeight()
{
    return height;
}
int tft_getWidth()
{
    return width;
}
void tft_drawNetworkInfo(char *ip)
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

void setCursor(int x, int y, int fontsize)
{
    tft.setCursor(x, y, fontsize);
}

void printTxt(int x, int y, int fontsize, const char *txt)
{
    tft.setCursor(x, y, fontsize);
    tft.print(txt);
}