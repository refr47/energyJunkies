#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t g_appMutex;
extern SemaphoreHandle_t g_tftMutex;

bool appSyncInit();