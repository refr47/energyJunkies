#include "app_sync.h"
#include "utils.h"

#define TAG_APP_SYNC "APP_SYNC"

SemaphoreHandle_t g_appMutex       = nullptr;
SemaphoreHandle_t g_tftMutex       = nullptr;
SemaphoreHandle_t g_ajaxMutex      = nullptr;
SemaphoreHandle_t g_shellyMutex    = nullptr;
SemaphoreHandle_t g_jsonMutex      = nullptr;
SemaphoreHandle_t g_jsonResultMutex = nullptr;

bool appSyncInit()
{
    g_appMutex = xSemaphoreCreateMutex();
    if (g_appMutex == nullptr)
    {
        LOG_ERROR(TAG_APP_SYNC, "CRITICAL: appMutex creation failed!");
        return false;
    }

    g_tftMutex = xSemaphoreCreateMutex();
    if (g_tftMutex == nullptr)
    {
        vSemaphoreDelete(g_appMutex);  /* Rollback on failure */
        g_appMutex = nullptr;
        LOG_ERROR(TAG_APP_SYNC, "CRITICAL: tftMutex creation failed!");
        return false;
    }

    g_ajaxMutex = xSemaphoreCreateMutex();
    if (g_ajaxMutex == nullptr)
    {
        LOG_ERROR(TAG_APP_SYNC, "CRITICAL: ajaxMutex creation failed!");
        return false;
    }

    g_shellyMutex = xSemaphoreCreateMutex();
    if (g_shellyMutex == nullptr)
    {
        LOG_ERROR(TAG_APP_SYNC, "CRITICAL: shellyMutex creation failed!");
        return false;
    }

    g_jsonMutex = xSemaphoreCreateMutex();
    if (g_jsonMutex == nullptr)
    {
        LOG_ERROR(TAG_APP_SYNC, "CRITICAL: jsonMutex creation failed!");
        return false;
    }

    g_jsonResultMutex = xSemaphoreCreateMutex();
    if (g_jsonResultMutex == nullptr)
    {
        LOG_ERROR(TAG_APP_SYNC, "CRITICAL: jsonResultMutex creation failed!");
        return false;
    }

    LOG_INFO(TAG_APP_SYNC, "All 7 RTOS mutexes initialized OK");
    return true;
}

void appSyncCleanup()
{
    if (g_appMutex)      { vSemaphoreDelete(g_appMutex);      g_appMutex      = nullptr; }
    if (g_tftMutex)      { vSemaphoreDelete(g_tftMutex);      g_tftMutex      = nullptr; }
    if (g_ajaxMutex)     { vSemaphoreDelete(g_ajaxMutex);     g_ajaxMutex     = nullptr; }
    if (g_shellyMutex)   { vSemaphoreDelete(g_shellyMutex);   g_shellyMutex   = nullptr; }
    if (g_jsonMutex)     { vSemaphoreDelete(g_jsonMutex);     g_jsonMutex     = nullptr; }
    if (g_jsonResultMutex){ vSemaphoreDelete(g_jsonResultMutex); g_jsonResultMutex = nullptr; }
    LOG_INFO(TAG_APP_SYNC, "All RTOS mutexes deleted");
}

bool appSyncMutexReady()
{
    return (g_appMutex != nullptr &&
            g_tftMutex != nullptr &&
            g_ajaxMutex != nullptr &&
            g_shellyMutex != nullptr &&
            g_jsonMutex != nullptr &&
            g_jsonResultMutex != nullptr);
}
