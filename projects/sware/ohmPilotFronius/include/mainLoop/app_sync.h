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

// KR-2: Fine-grained mutexes per data domain
// guard webSockData telemetry (states, temp, pid, log, mb, amis, fronius)
extern SemaphoreHandle_t g_dataMutex;
// guard webSockData.setupData (config + prefs)
extern SemaphoreHandle_t g_configMutex;
// guard pinManager + alarmContainer (PID output state)
extern SemaphoreHandle_t g_pidOutMutex;

bool appSyncInit();
void appSyncCleanup();                    // Mutexes bei tiefstem Reset freigeben
bool  appSyncMutexReady();               // Alle Mutexes auf != nullptr prüfen
