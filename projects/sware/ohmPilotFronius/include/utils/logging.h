#pragma once
#include <Arduino.h>
#include <esp_log.h>
#include "cardRW.h"
#ifndef TAG
#define TAG "ENERGIEJUNKIES"
#endif

void logging_init();

int sdCardLogOutput(const char *format, va_list args);
