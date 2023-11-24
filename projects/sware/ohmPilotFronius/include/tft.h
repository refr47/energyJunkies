#pragma once
#include <TFT_eSPI.h>
#include "pin_config.h"
#include "defines.h"

void tft_init();
void tft_printSetup();
TFT_eSPI &tft_getRoot();
int tft_getHeight();
int tft_getWidth();
void tft_print_txt(int num, ...);
void tft_showAvailableNetworks(int num, ...);
void tft_drawNetworkInfo(char *ip, const char *essid);
void tft_printKeyValue(const char *key, const char *value, u_int16_t txtColor);
void tft_setCursor(int x, int y, int fontsize = 4);

void tft_printInfo(const char *txt, bool newLine = true);
void tft_clearScreen();
void tft_updateTime(char *curTime);
void tft_drawInfo(TEMPERATURE &temp, MB_CONTAINER &modb, PID_CONTAINER &pidC);
void tft_drawInfoNoModbus(TEMPERATURE &temp);

#ifdef EJ
void tft_print_test(int yLine, int offsetX1, int offsetX2, u_int16_t txtColor, const char *key, const char *value);

#endif