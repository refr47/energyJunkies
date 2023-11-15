#pragma once
#include <TFT_eSPI.h>
#include "pin_config.h"
#include "defines.h"

#define NONE_ 0
#define ALARM_ 1 
#define DONE_  2

void tft_init();
void tft_printSetup();
TFT_eSPI &tft_getRoot();
int tft_getHeight();
int tft_getWidth();
void tft_print_txt(int num, ...);
void tft_showAvailableNetworks(int num, ...);
void tft_drawNetworkInfo(char *ip, const char *essid);
void tft_printKeyValue(const char *key, const char *value,char valAsAlarm=NONE_);
void tft_setCursor(int x, int y, int fontsize = 4);
void tft_printTxt(int x, int y, int fontsize = 4, const char *txt = "dummy");
void tft_printInfo(const char *txt, bool newLine = true);
void tft_clearScreen();
void tft_drawInfo(TEMPERATURE &temp, MB_CONTAINER &modb, PID_CONTAINER &pidC);
