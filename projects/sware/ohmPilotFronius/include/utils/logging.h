#pragma once
#include <Arduino.h>
#include <esp_log.h>
#include "cardRW.h"
#ifndef TAG
#define TAG "E-JUNKIES"
#endif

#define LOGFILE_SIZE 1024 * 1024

void logging_init();

int sdCardLogOutput(const char *format, va_list args);
