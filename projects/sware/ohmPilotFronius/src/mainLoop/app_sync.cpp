#include "app_sync.h"

SemaphoreHandle_t g_appMutex = nullptr;
SemaphoreHandle_t g_tftMutex = nullptr;

bool appSyncInit()
{
    g_appMutex = xSemaphoreCreateMutex();
    g_tftMutex = xSemaphoreCreateMutex();

    return (g_appMutex != nullptr) && (g_tftMutex != nullptr);
}