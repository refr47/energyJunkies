
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
#define FONTSIZE_2_ONE_LINE FONTSIZE_2 * 6
#define HEADER "Energie-Junkies"
#define FONT_WIDTH 7

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
    tft.setTextColor(TFT_RED);
    for (int jj = 0; jj < 5; jj++)
    {
        tft.setCursor(10 * jj, (jj * 10) + 60, 4);
        tft.print("E-Harvester .....");
        if (jj > 2)
            tft.setTextColor(TFT_BLUE);
        // delay(3000);
    }
    delay(4000);
    tft.setTextColor(TFT_WHITE);
    tft_clearScreen();
    /*  tft.setRotation(0);
     tft_clearScreen(); */
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
    currentLine = 0;
    tft_printInfo(HEADER);
    /* DBGf("Clear screen ");
    delay(4000); */
}
void tft_clearScreenFrom(int yFrom)
{

    tft.fillRect(0, yFrom, width, height, TFT_BLACK); // Bild ausschalten
    currentLine = yFrom % FONTSIZE_2;
    DBGf("tft-clearScreen with currentLine: %d", currentLine);
}

void tft_print_txt(int num, ...)
{
    va_list valist;
    va_start(valist, num);
    char *msg;

    for (int i = 0; i < num; i++)
    {
        msg = va_arg(valist, char *);

        /*  tft_printTxt(0, 0, FONTSIZE_2, (const char *)"Init Network ... ");
         if (i == 1)
         {
             tft_clearScreenFrom(FONTSIZE_2_ONE_LINE * 2);
         } */
        DBGf("          ::::tft_print_txt: %d,  text: %s", currentLine, msg);
        tft_printTxt(5, FONTSIZE_2_ONE_LINE * currentLine++, FONTSIZE_2, (const char *)msg);
    }
    va_end(valist);
}
void tft_showAvailableNetworks(int num, ...)
{
    va_list valist;
    va_start(valist, num);
    char *msg;

    for (int i = 0; i < num; i++)
    {
        msg = va_arg(valist, char *);

        /*    tft_printTxt(0, 0, FONTSIZE_2, (const char *)"Init Network ... ");
           if (i == 1)
           {
               tft_clearScreenFrom(FONTSIZE_2_ONE_LINE * 2);
           } */
        tft_printTxt(5, FONTSIZE_2_ONE_LINE * currentLine++, FONTSIZE_2, msg);
    }
    va_end(valist);
}

void tft_drawNetworkInfo(char *ip, const char *essid)
{
    tft_clearScreen();
    tft.setTextColor(TFT_WHITE);
    tft_printInfo("WLAN ");
    if (ip != NULL)
    {
        tft_printInfo("Connected ");
        /* tft_printKeyValue("ESSID: ", essid);
        tft_printKeyValue("IP: ", ip); */
        // tft.pushImage(0, 0, 24, 24, wlanPic24);
    }
    else
    {
        tft_printInfo("Wlan nicht gefunden!");
    }
}

void tft_printInfo(const char *txt, bool newLine)
{
    if (!newLine)
        tft.fillRect(0, FONTSIZE_2_ONE_LINE * currentLine, width, FONTSIZE_2_ONE_LINE, TFT_BLACK);
    tft_printTxt(5, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, txt);
    if (newLine)
        currentLine++;
}

void tft_setCursor(int x, int y, int fontsize)
{
    tft.setCursor(x, y, fontsize);
}

void tft_printKeyValue(const char *key, const char *value)
{
    // DBGf("          ::::tft_printKeyValue: %d,  text: %s", currentLine, key);

    tft_printTxt(5, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, key);

    tft_printTxt(tft.getCursorX() + 10, FONTSIZE_2_ONE_LINE * (currentLine++), FONTSIZE_2, value);
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

/* void tft_printInfo(const char *txt, bool newLine)
{
    if (!newLine)
        tft.fillRect(0, FONTSIZE_2_ONE_LINE * currentLine, width, FONTSIZE_2_ONE_LINE, TFT_BLACK);
    tft_printTxt(5, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, txt);
    if (newLine)
        currentLine++;
} */

void tft_prinBlock(int offsetX1, int offsetX2, bool alert, const char *key, const char *value)
{
    if (alert)
        tft.setTextColor(TFT_RED);
    tft_printTxt(offsetX1, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, key);
    tft.fillRect(tft.getCursorX(), tft.getCursorY(), width - tft.getCursorX(), FONTSIZE_2_ONE_LINE, TFT_BLACK);

    tft_printTxt(offsetX2, FONTSIZE_2_ONE_LINE * (currentLine), FONTSIZE_2, value);
    if (alert)
        tft.setTextColor(TFT_WHITE);
}

static char buf[50];
static int saveCurLine;

void tft_drawInfo(TEMPERATURE &temp, MB_CONTAINER &modb, PID_CONTAINER &pidC)
{
    saveCurLine = currentLine;
    bool setAlert = false;
    tft_printTxt(5, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, "Temperatur");
    tft_printTxt(134, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, "Energie");
    ++currentLine;
    sprintf(buf, "%.2f", temp.sensor1);
    tft_prinBlock(14, 75, setAlert, "Sensor 1", buf);
    if (modb.meterValues.data.acCurrentPower >= 0.0)
        setAlert = true;
    DBGf("ALERT: %x", setAlert);
    sprintf(buf, "%.2f", modb.meterValues.data.acCurrentPower);
    setAlert = false;
    tft_prinBlock(148, 230, setAlert, "Produktion", buf);
    ++currentLine;
    sprintf(buf, "%.2f", temp.sensor2);
    tft_prinBlock(14, 75, setAlert, "Sensor 2", buf);
    sprintf(buf, "%.2f", modb.meterValues.data.acTotalEnergyExp);
    tft_prinBlock(148, 230, setAlert, "Einspeisung", buf);
    ++currentLine;
    sprintf(buf, "%.2f", modb.meterValues.data.acTotalEnergyImp);
    tft_prinBlock(148, 230, setAlert, "Bezug", buf);
    currentLine += 2;
    /*   tft_printInfo("Energie");
      DBGf("currentBlock y %d", tft.getCursorY());
      sprintf(buf, "%.2f", modb.meterValues.data.acCurrentPower);
      tft_prinBlock("Aktuelle Produktion", buf);
      sprintf(buf, "%.2f", modb.meterValues.data.acTotalEnergyExp);
      tft_prinBlock("Aktuelle EInspeisung", buf);
      sprintf(buf, "%.2f", modb.meterValues.data.acTotalEnergyImp);
      tft_prinBlock("Aktueller Bezug", buf); */
    tft_printInfo("Bufferspeicher");
    sprintf(buf, "%.2f", modb.meterValues.data.acTotalEnergyImp);
    tft_printInfo("Speicher");

    currentLine = saveCurLine;
}