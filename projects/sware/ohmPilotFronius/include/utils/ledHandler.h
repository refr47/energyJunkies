#pragma once
#include "pin_config.h"

void ledHandler_init();
uint8_t ledHandler_blink();
void ledHandler_showModbusError(bool enable);
void ledHandler_showCardReaderError(bool enable);
void ledHandler_showTemperaturError(bool enable);
void ledHandler_showNetworkError(bool enable);
void ledHandler_showPWM(unsigned);
unsigned ledHandler_getPWM();