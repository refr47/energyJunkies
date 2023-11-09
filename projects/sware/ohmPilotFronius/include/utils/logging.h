#pragma once
#include <Arduino.h>
#include <esp_log.h>
#include "cardRW.h"

#define TAG "ENERGIEJUNKIES"
#define LOG_LEVEL ESP_LOG_INFO
#define MY_ESP_LOG_LEVEL ESP_LOG_WARN


void logging_init();

int sdCardLogOutput(const char *format, va_list args);
