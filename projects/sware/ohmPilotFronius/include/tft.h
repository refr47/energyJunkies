#ifndef __TFT_H__
#define __TFT_H__
#include <TFT_eSPI.h>
#include "pin_config.h"

void tft_init();
void tft_printSetup();
TFT_eSPI &tft_getRoot();
int tft_getHeight();
int tft_getWidth();
void tft_initNetwork(int num, ...);
void tft_showAvailableNetworks(int num, ...);
void tft_drawNetworkInfo(char *ip);
void tft_setCursor(int x, int y, int fontsize = 4);
void tft_printTxt(int x, int y, int fontsize = 4, const char *txt = "dummy");
void tft_clearScreen();

#endif