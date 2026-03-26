#include "app_state.h"
#include <cstring>

APP_RUNTIME g_app;

bool appStateInit()
{
    memset(&g_app, 0, sizeof(g_app));
    g_app.networkCredentialsInEEprom = true;
    return true;
}

void appLock()
{
    if (g_appMutex != nullptr)
    {
        xSemaphoreTake(g_appMutex, portMAX_DELAY);
    }
}

void appUnlock()
{
    if (g_appMutex != nullptr)
    {
        xSemaphoreGive(g_appMutex);
    }
}