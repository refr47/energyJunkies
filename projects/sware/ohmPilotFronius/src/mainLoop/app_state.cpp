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

bool appLock(uint32_t timeout_ms)
{
    if (g_appMutex == nullptr)
    {
        LOG_DEBUG("MUTEX", "Versuchter Lock ohne initialisierten Mutex!");
        return false;
    }

    return xSemaphoreTake(g_appMutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
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

// --- KR-2: Domain-specific fine-grained locks ---
// Lock ordering: g_dataMutex > g_configMutex > g_pidOutMutex

bool appLockData(uint32_t timeout_ms)
{
    if (g_dataMutex == nullptr)
    {
        LOG_DEBUG("MUTEX", "Versuchter Data-Lock ohne initialisierten Mutex!");
        return false;
    }

    return xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void appUnlockData()
{
    if (g_dataMutex != nullptr)
    {
        if (xSemaphoreGetMutexHolder(g_dataMutex) == xTaskGetCurrentTaskHandle())
        {
            xSemaphoreGive(g_dataMutex);
        }
        else
        {
            LOG_DEBUG("MUTEX", "Versuchter Data-Unlock ohne Besitz!");
        }
    }
}

bool appLockConfig(uint32_t timeout_ms)
{
    if (g_configMutex == nullptr)
    {
        LOG_DEBUG("MUTEX", "Versuchter Config-Lock ohne initialisierten Mutex!");
        return false;
    }

    return xSemaphoreTake(g_configMutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void appUnlockConfig()
{
    if (g_configMutex != nullptr)
    {
        if (xSemaphoreGetMutexHolder(g_configMutex) == xTaskGetCurrentTaskHandle())
        {
            xSemaphoreGive(g_configMutex);
        }
        else
        {
            LOG_DEBUG("MUTEX", "Versuchter Config-Unlock ohne Besitz!");
        }
    }
}

bool appLockPidOut(uint32_t timeout_ms)
{
    if (g_pidOutMutex == nullptr)
    {
        LOG_DEBUG("MUTEX", "Versuchter PidOut-Lock ohne initialisierten Mutex!");
        return false;
    }

    return xSemaphoreTake(g_pidOutMutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void appUnlockPidOut()
{
    if (g_pidOutMutex != nullptr)
    {
        if (xSemaphoreGetMutexHolder(g_pidOutMutex) == xTaskGetCurrentTaskHandle())
        {
            xSemaphoreGive(g_pidOutMutex);
        }
        else
        {
            LOG_DEBUG("MUTEX", "Versuchter PidOut-Unlock ohne Besitz!");
        }
    }
}