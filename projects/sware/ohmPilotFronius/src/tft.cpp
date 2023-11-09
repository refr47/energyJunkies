
#include <SPI.h>
#include "debugConsole.h"
#include "tft.h"
#include "img/wlanPic24.h"

/*

    fontsize: 4
    30px per Line
    fontsize: 3  does not work
    fontsize: 2:
*/
/* GLOBAL VARS*/

#define FONTSIZE_4 4
#define FONTSIZE_4_ONE_LINE 30
#define FONTSIZE_2 2
#define FONTSIZE_2_ONE_LINE 14

static TFT_eSPI tft = TFT_eSPI();
static int height = 0, width = 0;
unsigned int currentLine = 0;

void tft_init()
{
    tft.begin();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    ledcSetup(0, 2000, 8);
    ledcAttachPin(PIN_LCD_BL, 0);
    ledcWrite(0, 255);

    tft.setSwapBytes(true);
    width = tft.width();
    height = tft.height();
    DBGf("TFT Width %d,Height: %d", width, height);

    tft.setRotation(1);
    for (int jj = 0; jj < 5; jj++)
    {
        tft.setCursor(10 * jj, (jj * 10) + 30, 4);
        tft.print("INIT .....");
        // delay(3000);
    }
    delay(1000);
    tft.setRotation(0);
}
int8_t getPinName(int8_t pin, setup_t &user)
{
    // For ESP32 and RP2040 pin labels on boards use the GPIO number
    if (user.esp == 0x32 || user.esp == 0x2040)
        return pin;
    if (user.esp == 0x8266)
    {
        // For ESP8266 the pin labels are not the same as the GPIO number
        // These are for the NodeMCU pin definitions:
        //        GPIO       Dxx
        if (pin == 16)
            return 0;
        if (pin == 5)
            return 1;
        if (pin == 4)
            return 2;
        if (pin == 0)
            return 3;
        if (pin == 2)
            return 4;
        if (pin == 14)
            return 5;
        if (pin == 12)
            return 6;
        if (pin == 13)
            return 7;
        if (pin == 15)
            return 8;
        if (pin == 3)
            return 9;
        if (pin == 1)
            return 10;
        if (pin == 9)
            return 11;
        if (pin == 10)
            return 12;
    }

    if (user.esp == 0x32F)
        return pin;

    return pin; // Invalid pin
}

void tft_printSetup()
{
    setup_t user;
    tft.getSetup(user);
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP8266)
    if (user.esp < 0x32F000 || user.esp > 0x32FFFF)
    {
        DBGf("Frequency    =  %d MHz", ESP.getCpuFreqMHz());
    }
#endif

    DBGf("Check tft SPI");
    if (user.pin_tft_mosi != -1)
    {
        DBGf("MOSI    =  %d", getPinName(user.pin_tft_mosi, user));
    }
    if (user.pin_tft_miso != -1)
    {

        DBGf("MISO    =  %d", getPinName(user.pin_tft_miso, user));
    }
    if (user.pin_tft_clk != -1)
    {

        DBGf("SCK    =  %d", getPinName(user.pin_tft_clk, user));
    }

    String pinNameRef = "GPIO ";
#ifdef ARDUINO_ARCH_ESP8266
    pinNameRef = "PIN_D";
#endif
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

void tft_clearScreen()
{
    tft.fillRect(0, 0, width, height, TFT_BLACK); // Bild ausschalten
}
void tft_clearScreenFrom(int yFrom)
{
    tft.fillRect(0, yFrom, width, height, TFT_BLACK); // Bild ausschalten
}
void tft_initNetwork(int num, ...)
{
    va_list valist;
    va_start(valist, num);
    char *msg;

    for (int i = 0; i < num; i++)
    {
        msg = va_arg(valist, char *);

        tft_printTxt(0, 0, FONTSIZE_2, "Init Network ... ");
        if (i == 1)
        {
            tft_clearScreenFrom(FONTSIZE_2_ONE_LINE * 2);
        }
        tft_printTxt(5, FONTSIZE_2_ONE_LINE * (i + 1), FONTSIZE_2, msg);
    }
    va_end(valist);
    currentLine = num;
}
void tft_showAvailableNetworks(int num, ...)
{
    va_list valist;
    va_start(valist, num);
    char *msg;

    for (int i = 0; i < num; i++)
    {
        msg = va_arg(valist, char *);

        tft_printTxt(0, 0, FONTSIZE_2, "Init Network ... ");
        if (i == 1)
        {
            tft_clearScreenFrom(FONTSIZE_2_ONE_LINE * 2);
        }
        tft_printTxt(5, FONTSIZE_2_ONE_LINE * currentLine++, FONTSIZE_2, msg);
    }
    va_end(valist);
}

void tft_drawNetworkInfo(char *ip)
{
    tft.setTextColor(TFT_WHITE);
    if (ip != NULL)
    {
        // tft.pushImage(0, 0, 24, 24, wlanPic24);
        tft.fillRect(0, FONTSIZE_2_ONE_LINE * 2, width, FONTSIZE_2_ONE_LINE, TFT_BLACK);
        tft_printTxt(0, FONTSIZE_2_ONE_LINE * 2, FONTSIZE_2, "IP: ");
        tft.setCursor(30, FONTSIZE_2_ONE_LINE * 2, FONTSIZE_2);
        tft.println(ip);
    }
}

void tft_setCursor(int x, int y, int fontsize)
{
    tft.setCursor(x, y, fontsize);
}

void tft_printTxt(int x, int y, int fontsize, const char *txt)
{
    tft.setCursor(x, y, fontsize);
    tft.print(txt);
}

uint16_t screenBuffer[TFT_WIDTH * TFT_HEIGHT]; // Speicher für Bildschirminhalt

void backupScreen()
{
    tft.readRect(0, 0, width, height, screenBuffer); // Bildschirminhalt in den Speicher kopieren
}

void restoreScreen()
{
    tft.pushRect(0, 0, width, height, screenBuffer); // Bildschirminhalt vom Speicher auf den Bildschirm kopieren
}

void displayErrorMessage(const char *message)
{
    backupScreen(); // Bildschirminhalt sichern

    tft.fillScreen(TFT_BLACK);   // Bildschirmhintergrund löschen
    tft.setTextColor(TFT_WHITE); // Textfarbe festlegen
    tft.setTextSize(2);          // Textgröße festlegen
    tft.setCursor(10, 10);       // Position des Textes auf dem Bildschirm festlegen
    tft.println(message);        // Fehlermeldung anzeigen
}
