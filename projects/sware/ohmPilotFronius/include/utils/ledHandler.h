#pragma once
#include "pin_config.h"

void ledHandler_init();
void ledHandler_blink();
void ledHandler_showModbusError(bool enable);
void ledHandler_showCardReaderError(bool enable);
void ledHandler_showTemperaturError(bool enable);