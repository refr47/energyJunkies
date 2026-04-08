#include "app_state.h"
#include "utils.h"
#include <cstring>

APP_RUNTIME g_app;

bool appStateInit()
{
    memset(&g_app, 0, sizeof(g_app));
    g_app.networkCredentialsInEEprom = true;
    utils_logInit(g_app.webSockData.logBuffer);
    g_app.webSockData.logBuffer.active = true;

    return true;
}

bool appLock(uint32_t timeout_ms=100)
{
    if (g_appMutex != nullptr)
    {
        return xSemaphoreTake(g_appMutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    }
    return false;
}

void appUnlock()
{
    if (g_appMutex != nullptr)
    {
        // xSemaphoreGetMutexHolder prüft, ob DIESER Task den Mutex wirklich hält
        if (xSemaphoreGetMutexHolder(g_appMutex) == xTaskGetCurrentTaskHandle())
        {
            xSemaphoreGive(g_appMutex);
        }
        else
        {
            // Optional: Logge eine Warnung, aber crashe nicht!
             LOG_DEBUG("MUTEX", "Versuchter Unlock ohne Besitz!");
        }
    }
}