#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t g_appMutex;
extern SemaphoreHandle_t g_tftMutex;

// Modul-weit exportierte Mutex-Handles (zentral durch appSyncInit() erstellt)
extern SemaphoreHandle_t g_ajaxMutex;
extern SemaphoreHandle_t g_shellyMutex;
extern SemaphoreHandle_t g_jsonMutex;
extern SemaphoreHandle_t g_jsonResultMutex;

bool appSyncInit();
void appSyncCleanup();                    // Mutexes bei tiefstem Reset freigeben
bool  appSyncMutexReady();               // Alle Mutexes auf != nullptr prüfen
