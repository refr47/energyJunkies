
#include <SPI.h>
#include "debugConsole.h"
#include "tft.h"
#include "img/wlanPic24.h"
#include "utils.h"
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
#define FONT_WIDTH 7
#define FONTSIZE_2_ONE_LINE FONTSIZE_2 * 7
#define HEADER "Energie-Junkies"
#define HEADER_LEN strlen(HEADER) * FONTSIZE_2_ONE_LINE

#define DRAW_INFO_COL1 5
#define DRAW_INFO_COL2 148

#define DRAW_INFO_COL1_2 75
#define DRAW_INFO_COL2_2 230

// #include <User_Setups/Setup206_LilyGo_T_Display_S3.h>
static TFT_eSPI tft = TFT_eSPI();
static int height = 0, width = 0;
unsigned int currentLine = 0;

static void tft_printTextToPos(int x, int y, int fontsize, const char *txt, u_int16_t colourText);
static void tft_prinBlock(int offsetX1, int offsetX2, u_int16_t txtColor, const char *key, const char *value);

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
        if (jj > 1)
            tft.setTextColor(TFT_YELLOW);
        // delay(3000);
    }
    delay(4000);
    tft.setTextColor(TFT_WHITE);
    tft_clearScreen();
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
void tft_clearScreen()
{
    tft.fillRect(0, 0, width, height, TFT_BLACK); // Bild ausschalten
    currentLine = 0;

    int offset = (width - (strlen(HEADER) * FONT_WIDTH)) / 2;
    tft_printTextToPos(offset, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, HEADER, TFT_GREEN);
    // tft_printInfo(HEADER);

    ++currentLine;
    /* DBGf("Clear screen ");
    delay(4000); */
}

void tft_updateTime(char *curTime)
{

    // clear first line
    tft.fillRect(0, 0, width, FONTSIZE_2_ONE_LINE * 2, TFT_BLACK);
    tft_printTextToPos(14, FONTSIZE_2_ONE_LINE, FONTSIZE_2, HEADER, TFT_GREEN);
    tft_printTextToPos(150, FONTSIZE_2_ONE_LINE, FONTSIZE_2, curTime, TFT_WHITE);
}

inline int tft_getHeight()
{
    return height;
}
inline int tft_getWidth()
{
    return width;
}
TFT_eSPI &tft_getRoot()
{
    return tft;
}

void tft_print_txt(int num, ...)
{
    va_list valist;
    va_start(valist, num);
    char *msg;

    for (int i = 0; i < num; i++)
    {
        msg = va_arg(valist, char *);

        /*  tft_printTextToPos(0, 0, FONTSIZE_2, (const char *)"Init Network ... ");
         if (i == 1)
         {
             tft_clearScreenFrom(FONTSIZE_2_ONE_LINE * 2);
         } */
        DBGf("          ::::tft_print_txt: %d,  text: %s", currentLine, msg);
        tft_printTextToPos(5, FONTSIZE_2_ONE_LINE * currentLine++, FONTSIZE_2, (const char *)msg, TFT_WHITE);
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

        /*    tft_printTextToPos(0, 0, FONTSIZE_2, (const char *)"Init Network ... ");
           if (i == 1)
           {
               tft_clearScreenFrom(FONTSIZE_2_ONE_LINE * 2);
           } */
        tft_printTextToPos(5, FONTSIZE_2_ONE_LINE * currentLine++, FONTSIZE_2, msg, TFT_WHITE);
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
    tft_printTextToPos(5, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, txt, TFT_WHITE);
    if (newLine)
        currentLine++;
}

void tft_setCursor(int x, int y, int fontsize)
{
    tft.setCursor(x, y, fontsize);
}

uint16_t screendisplayBufferfer[TFT_WIDTH * TFT_HEIGHT]; // Speicher für Bildschirminhalt

void backupScreen()
{
    tft.readRect(0, 0, width, height, screendisplayBufferfer); // Bildschirminhalt in den Speicher kopieren
}

void restoreScreen()
{
    tft.pushRect(0, 0, width, height, screendisplayBufferfer); // Bildschirminhalt vom Speicher auf den Bildschirm kopieren
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


void tft_printKeyValue(const char *key, const char *value, u_int16_t txtColor)
{
    
    tft_printTextToPos(5, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, key, TFT_WHITE);

    if (txtColor == TFT_WHITE)
        tft.setTextColor(txtColor);
  
    tft_printTextToPos(tft.getCursorX() + 10, FONTSIZE_2_ONE_LINE * (currentLine++), FONTSIZE_2, value, txtColor);
    tft.setTextColor(TFT_WHITE);
}

/* void tft_printInfo(const char *txt, bool newLine)
{
    if (!newLine)
        tft.fillRect(0, FONTSIZE_2_ONE_LINE * currentLine, width, FONTSIZE_2_ONE_LINE, TFT_BLACK);
    tft_printTextToPos(5, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, txt);
    if (newLine)
        currentLine++;
} */

static void tft_clearScreenFrom(int yFrom)
{

    tft.fillRect(0, yFrom, width, height, TFT_BLACK); // Bild ausschalten
    currentLine = yFrom % FONTSIZE_2;
    DBGf("tft-clearScreen with currentLine: %d", currentLine);
}

static inline void tft_printTextToPos(int x, int y, int fontsize, const char *txt, u_int16_t colourText)
{
    tft.setTextColor(colourText);
    tft.setCursor(x, y, fontsize);
    tft.print(txt);
}

static inline void tft_prinBlock(int offsetX1, int offsetX2, u_int16_t txtColor, const char *key, const char *value)
{

    // DBGf("tft_printBlock, textC: %x", txtColor);
    tft_printTextToPos(offsetX1, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, key, TFT_WHITE);
    tft.fillRect(offsetX2, tft.getCursorY(), width - tft.getCursorX(), FONTSIZE_2_ONE_LINE, TFT_BLACK);

    // tft.setTextColor(txtColor);
    tft_printTextToPos(offsetX2, FONTSIZE_2_ONE_LINE * (currentLine), FONTSIZE_2, value, txtColor);

    // tft.setTextColor(TFT_WHITE);
}


#ifdef EJ

void tft_print_test(int yLine, int offsetX1, int offsetX2, u_int16_t txtColor, const char *key, const char *value) {
    tft_printTextToPos(offsetX1, FONTSIZE_2_ONE_LINE * yLine, FONTSIZE_2, key, TFT_WHITE);
    tft.fillRect(offsetX2, tft.getCursorY(), width - tft.getCursorX(), FONTSIZE_2_ONE_LINE, TFT_BLACK);

    // tft.setTextColor(txtColor);
    tft_printTextToPos(offsetX2, FONTSIZE_2_ONE_LINE * (yLine), FONTSIZE_2, value, txtColor);
}
#endif
/*
 double capacity;            // storage capacity in Wh
    double chargeRateLimit;     // max. charge rate
    double dischargeRateLimit;  // max discharge rate
*/

static char displayBuffer[50];
static int saveCurLine;

void tft_drawInfo(TEMPERATURE &temp, MB_CONTAINER &modb, PID_CONTAINER &pidC)
{

    saveCurLine = currentLine;

    char formatBuffer[25]; // format W | kW
    u_int16_t txtColor = TFT_WHITE;
    ++currentLine;
    tft_printTextToPos(5, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, "Temperatur", TFT_SKYBLUE);
    tft_printTextToPos(134, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, "Energie", TFT_SKYBLUE);
    ++currentLine;
    sprintf(displayBuffer, "%.2f", temp.sensor1);
    tft_prinBlock(DRAW_INFO_COL1, DRAW_INFO_COL1_2, txtColor, "Sensor 1", displayBuffer);
    // production LINE 1
    if (modb.inverterSumValues.data.acCurrentPower >= 0.0)
        txtColor = TFT_WHITE;
    else
        txtColor = TFT_GREEN;
    sprintf(displayBuffer, "%s", util_format_Watt_kWatt(modb.inverterSumValues.data.acCurrentPower, formatBuffer));
    tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Produktion", displayBuffer);
    ++currentLine;

    // sensor 2 + Einspeisung/verbrauch LINE 2
    sprintf(displayBuffer, "%.2f", temp.sensor2);
    tft_prinBlock(DRAW_INFO_COL1, DRAW_INFO_COL1_2, txtColor, "Sensor 2", displayBuffer);

    // smart meter delivers sometimes not valid values like -32456 W einspeisung (!!)
    if (modb.meterValues.data.acCurrentPower > 0.0 || (modb.inverterSumValues.data.acCurrentPower + modb.meterValues.data.acCurrentPower > 0))
    {

        if (modb.meterValues.data.acCurrentPower > 0.0)
        {
            txtColor = TFT_RED;
            sprintf(displayBuffer, "%s", util_format_Watt_kWatt(modb.meterValues.data.acCurrentPower, formatBuffer));
            tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Bezug", displayBuffer);
        }
        else
        {
            txtColor = TFT_GREEN;
            // more energy is produced then consumend - negative values - for display: remove "-"
            sprintf(displayBuffer, "%s", util_format_Watt_kWatt(modb.meterValues.data.acCurrentPower * -1.0, formatBuffer));
            tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Einspeisung", displayBuffer);
        }

        // DBGf("=======================>>>============einspeiseTarif: %.2f  --- %s", modb.meterValues.data.acCurrentPower, displayBuffer);

        ++currentLine;

        // LINE 3 Verbrauch
        if (modb.inverterSumValues.data.acCurrentPower + modb.meterValues.data.acCurrentPower > 0.0)
            txtColor = TFT_RED;
        else
            txtColor = TFT_GREEN;

        // verbrauch
        sprintf(displayBuffer, "%s", util_format_Watt_kWatt(modb.inverterSumValues.data.acCurrentPower + modb.meterValues.data.acCurrentPower, formatBuffer));
        tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Verbrauch", displayBuffer);

        sprintf(displayBuffer, "%.2f", modb.meterValues.data.acTotalEnergyImp);
    }
    else
    {
        sprintf(displayBuffer, "%s", util_format_Watt_kWatt(0.00, formatBuffer));
        tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Einspeisung", displayBuffer);
        ++currentLine;
        sprintf(displayBuffer, "%s", util_format_Watt_kWatt(0.00, formatBuffer));
        tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Verbrauch", displayBuffer);
    }

    currentLine += 1;

    tft_printTextToPos(5, FONTSIZE_2_ONE_LINE * currentLine, FONTSIZE_2, "Speicher", TFT_SKYBLUE);
    tft_printTextToPos(134, FONTSIZE_2_ONE_LINE * currentLine++, FONTSIZE_2, "Pufferspeicher", TFT_SKYBLUE);
    txtColor = TFT_WHITE;
    sprintf(displayBuffer, "%s", util_format_Watt_kWatt(modb.akkuState.data.capacity, formatBuffer));
    tft_prinBlock(DRAW_INFO_COL1, DRAW_INFO_COL1_2, txtColor, "Kapazität", displayBuffer);
    sprintf(displayBuffer, "%s", pidC.PID_PIN1 == 1 ? "ein" : "aus");
    tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Phase 1", displayBuffer);
    ++currentLine;

    sprintf(displayBuffer, "%.2lf %", modb.akkuStr.data.chargeRate);
    tft_prinBlock(DRAW_INFO_COL1, DRAW_INFO_COL1_2, txtColor, "Laden", displayBuffer);
    sprintf(displayBuffer, "%s", pidC.PID_PIN2 == 1 ? "ein" : "aus");
    tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Phase 2", displayBuffer);
    ++currentLine;

    sprintf(displayBuffer, "%.2lf %", modb.akkuStr.data.stateOfCharge);
    tft_prinBlock(DRAW_INFO_COL1, DRAW_INFO_COL1_2, txtColor, "Stand", displayBuffer);
    if (pidC.mAnalogOut > 0.0)
        sprintf(displayBuffer, "%.2lf \%", (255.0 / pidC.mAnalogOut) * 100.0);
    else
        sprintf(displayBuffer, "%.2lf \%", 0.00);
    tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Phase 3", displayBuffer);
    ++currentLine;
    if (modb.akkuStr.data.dischargeRate > 0.0)
        txtColor = TFT_RED;
    sprintf(displayBuffer, "%.2lf \%", modb.akkuStr.data.dischargeRate);
    tft_prinBlock(DRAW_INFO_COL1, DRAW_INFO_COL1_2, txtColor, "Entladen", displayBuffer);
    txtColor = TFT_WHITE;
    sprintf(displayBuffer, "%s", util_format_Watt_kWatt(pidC.powerNotUseable, formatBuffer));
    tft_prinBlock(DRAW_INFO_COL2, DRAW_INFO_COL2_2, txtColor, "Reserviert", displayBuffer);
    ++currentLine;

    currentLine = saveCurLine;
}