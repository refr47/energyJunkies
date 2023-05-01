#ifndef __TFT_H__
#define __TFT_H__
#include <TFT_eSPI.h>

void tft_init();
TFT_eSPI &tft_getRoot();
int tft_getHeight();
int tft_getWidth();
void tft_drawNetworkInfo(char *ip);
void setCursor(int x, int y, int fontsize = 4);
void printTxt(int x, int y, int fontsize = 4, const char *txt = "dummy");

#endif