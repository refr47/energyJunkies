#include "app_services.h"

#include <Arduino.h>
#include <cstring>
#include <memory>

#include "app_state.h"
#include "app_sync.h"
#include "data/WebSockDataAccess.h"

#include "debugConsole.h"
#include "wlan.h"
#include "utils.h"
#include "tft.h"
#include "curTime.h"
#include "ledHandler.h"
#include "webSockets.h"
#include "logReader.h"
#include "eprom.h"
#include "hotUpdate.h"
#include "esp_wifi.h"

#ifdef MQTT
#include "mqtt.h"
#endif

#ifdef INFLUX
#include "influx.h"
#endif


#include "froniusSolarAPI.h"
// ── Energy-Source Abstraction ──
#include "energySource.h"
#ifdef FRONIUS_IV
#include "froniusHTTP.h"
#endif
#ifdef AMIS_READER_DEV
#include "devices/amisREST.h"
#endif
#ifdef HUAWEI_MODBUS
#include "huaweiModbus.h"
#endif
#ifdef LOXONE_API
#include "loxoneAsProvider.h"
#endif
#include "weather.h"
#ifdef CARD_READER
#include "cardRW.h"
#endif
#ifdef FRONIUS_IV
#include "modbusReader.h"
#endif

static constexpr uint32_t TEMPERATURE_OVERHEATED_WAIT_IN_SECS = 300;
static constexpr uint32_t NETWORK_RECOVERY_GRACE_CYCLES   = 2;

// --- Lock timeout constants (milliseconds) ---
// Small values keep critical sections short; timeouts >0 guarantee watchdog safety
static constexpr uint32_t LOCK_TIMEOUT_DATA_MS   = 50;  // dataMutex (telemetry)
static constexpr uint32_t LOCK_TIMEOUT_CONFIG_MS = 50;  // configMutex (config)
static constexpr uint32_t LOCK_TIMEOUT_PIDOUT_MS = 50;  // pidOutMutex (PID output)
static constexpr uint32_t LOCK_TIMEOUT_MULTI_MS  = 100; // for multi-lock critical sections
static void handleLockFailure(const char* context);
// --- Helper: mark network down with lock, fail-safe otherwise ---
static void tryMarkNetworkDown(const char* reason, const char* failMsg);




static bool networkIsAvailable()
{
    return WiFi.status() == WL_CONNECTED && ws_getNetworkOK();
}

static void stopHeating(const char* reason)
{
    // KR-2: needs dataMutex (webSockData) + pidOutMutex (pinManager)
    // Lock ordering: Data > PidOut
    // FIX (Deadlock): never call appLockData() again inside the failure branch,
    // because the '||' may have already acquired dataMutex before failing on pidOut.
    // Calling it again from the same thread = deadlock with non-recursive mutex.
    bool hasData   = appLockData(LOCK_TIMEOUT_MULTI_MS);
    bool hasPidOut = appLockPidOut(LOCK_TIMEOUT_MULTI_MS);
    if (!hasData || !hasPidOut)
    {
        if (hasPidOut) appUnlockPidOut();
        if (hasData)   appUnlockData();
        LOG_ERROR(TAG_APP_SERVICES, "stopHeating: could not acquire locks for: %s", reason);
        return;
    }

    g_app.pinManager.reset();
    g_app.webSockData.states.boilerHeating = false;
    g_app.webSockData.pidContainer.mAnalogOut = 0;
    g_app.webSockData.pidContainer.PID_PIN1 = 0;
    g_app.webSockData.pidContainer.PID_PIN2 = 0;
    appUnlockPidOut();
    appUnlockData();

    LOG_ERROR(TAG_APP_SERVICES, "%s - heating disabled", reason);
}

static void markNetworkDown(const char* reason)
{
    // NOTE: call only after appLockData() + appLockPidOut() acquired by caller.
    // Does NOT call locks itself to avoid nested-lock deadlock (KR-1/KR-2).
    LOG_ERROR(TAG_APP_SERVICES, "%s", reason);
    ledHandler_showNetworkError(true);
    g_app.webSockData.states.networkOK = false;
    g_app.webSockData.energyState.valid = false;
    g_app.pinManager.reset();
    g_app.webSockData.states.boilerHeating = false;
    g_app.webSockData.pidContainer.mAnalogOut = 0;
    g_app.webSockData.pidContainer.PID_PIN1 = 0;
    g_app.webSockData.pidContainer.PID_PIN2 = 0;
#ifdef MQTT
    g_app.webSockData.states.mqtt = false;
#endif
}

static int averageTemp()
{
    return (ws_getSensor1() + ws_getSensor2()) / 2;
}

void serviceClock()
{
    if (getCurrentTime(g_app.formatBuffer, FORMAT_CHAR_BUFFER_LEN))
    {
        if (xSemaphoreTake(g_tftMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            tft_updateTime(g_app.formatBuffer);
            xSemaphoreGive(g_tftMutex);
        }
    }
    else
    {
        if (xSemaphoreTake(g_tftMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            tft_updateTime("00:00:00");
            xSemaphoreGive(g_tftMutex);
        }
    }

    g_app.secondsCounter++;
    g_app.secondsCounter %= SECONDS_PER_DAY;
    ledHandler_blink();

    // IP-Adresse nur bei Fehler loggen
    // String s = WiFi.localIP().toString();
    // LOG_INFO(TAG_APP_SERVICES, "Current IP: %s", s.c_str());

#ifdef MQTT
    {
        // Retry MQTT while WLAN is available, even if the previous MQTT
        // connection failed. Publish the broker state, not the WLAN state.
        const bool mqttActive = WiFi.status() == WL_CONNECTED && mqtt_loop();
        if (appLockData(LOCK_TIMEOUT_DATA_MS))
        {
            g_app.webSockData.states.mqtt = mqttActive;
            appUnlockData();
        }
    }
#endif
}

unsigned servicePhasenSchnittBlink()
{

    // LOG_INFO(TAG_APP_SERVICES, "app_services::servicePhasenSchnittBlink ");
    return ledHandler_getPWM();
}

uint8_t serviceErrorBlink()
{
    return ledHandler_blink();
}

void serviceNetworkSupervisor()
{
    // -- Netzwerkpruefung: setupData unter Lock kopieren --
    Setup localSetup;
    {
        // KR-2: setupData is CONFIG domain
        if (appLockConfig(LOCK_TIMEOUT_CONFIG_MS))
        {
            localSetup = g_app.webSockData.setupData;
            appUnlockConfig();
        }
        else
        {
            handleLockFailure("Could not acquire lock to read setup data for network check");
            return;
        }
    }
    if (!wifi_isStillConnected(localSetup))
    {
        tryMarkNetworkDown("Network down, reconnect pending",
            "Network down, but could not acquire lock to update state");
        return;
    }

    // WLAN status does not establish MQTT or energy-source health.
    if (appLockData(LOCK_TIMEOUT_DATA_MS))
    {
        g_app.webSockData.states.networkOK = true;
        appUnlockData();
    }

    ledHandler_showNetworkError(false);

}

void serviceTemperature()
{
    if (!temp_getTemperature(g_app.webSockData.temperature))
    {
        ledHandler_showTemperaturError(true);
        // KR-2: this section accesses states/temperature (DATA) + pinManager/alarmContainer (PIDOUT)
        if (appLockData(LOCK_TIMEOUT_DATA_MS) && appLockPidOut(LOCK_TIMEOUT_PIDOUT_MS))
        {
            g_app.webSockData.states.tempUnderflow = false;
            g_app.webSockData.states.tempSensorOK = false;

            if (g_app.webSockData.temperature.sensor1 < 0 &&
                g_app.webSockData.temperature.sensor2 < 0)
            {
#ifdef MQTT
                if (g_app.webSockData.states.mqtt)
                {
                    mqtt_publish_alarm_temp(g_app.webSockData.temperature.sensor1,
                        g_app.webSockData.temperature.sensor2);
                }
#endif
                if (!g_app.webSockData.temperature.alarm)
                {
                    LOG_ERROR(TAG_APP_SERVICES, "Temperature sensors failed - heater off");
                    g_app.pinManager.reset();
                    g_app.alarmContainer.alarmTemp.alarmTemp = true;
                    g_app.alarmContainer.alarmTemp.overFlowHappenedAt = time_getTimeStamp();
                    g_app.webSockData.temperature.alarm = true;
                }
            }
            appUnlockPidOut();
            appUnlockData();
        }

        return;
    }

    ledHandler_showTemperaturError(false);

    const int tempAvg = averageTemp();
    // KR-2: this section only accesses states/temperature/setupData (DATA)
    if (appLockData(LOCK_TIMEOUT_DATA_MS))
    {
        g_app.webSockData.states.tempSensorOK = true;
        g_app.webSockData.states.tempUnderflow = false;

        if (tempAvg < g_app.webSockData.setupData.tempMinInGrad)
        {
            LOG_INFO(TAG_APP_SERVICES, "Temperature underflow detected: %d °C, setup: %d °C", tempAvg, g_app.webSockData.setupData.tempMinInGrad);
            g_app.webSockData.states.tempUnderflow = true;
        }

        if (tempAvg < g_app.webSockData.setupData.tempMaxAllowedInGrad)
        {
            g_app.webSockData.temperature.alarm = false;
        }

        appUnlockData();
    }
}

static void handleLockFailure(const char* context)
{
    // KR-2: pinManager is PIDOUT domain
    // FIX: use reasonable timeout so watchdog stays happy; log when lock fails
    bool acquired = appLockPidOut(LOCK_TIMEOUT_PIDOUT_MS);
    if (acquired)
    {
        g_app.pinManager.reset();
        appUnlockPidOut();
    }
    else
    {
        LOG_ERROR(TAG_APP_SERVICES, "handleLockFailure: pidOutMutex timeout — pinManager NOT reset: %s", context);
    }
}

// --- Helper: mark network down with lock, fail-safe otherwise ---
static void tryMarkNetworkDown(const char* reason, const char* failMsg)
{
    // KR-2: needs Data + PidOut for markNetworkDown
    if (!appLockData(LOCK_TIMEOUT_DATA_MS))
    {
        handleLockFailure(failMsg);
        return;
    }
    if (!appLockPidOut(LOCK_TIMEOUT_PIDOUT_MS))
    {
        appUnlockData();
        LOG_ERROR(TAG_APP_SERVICES, "%s", failMsg);
        return;
    }

    markNetworkDown(reason);
    appUnlockPidOut();
    appUnlockData();
}


// =============================================================================
// Energy service helpers (static)
// =============================================================================

namespace
{
// -----------------------------------------------------------------------------
// checkTempSensorAndReset – Gate: temp sensor OK?
// -----------------------------------------------------------------------------
static bool checkTempSensorAndReset()
{
    // 1. Read temp sensor status (DATA domain)
    bool tempOK = true;
    if (appLockData(LOCK_TIMEOUT_DATA_MS))
    {
        tempOK = g_app.webSockData.states.tempSensorOK;
        appUnlockData();
    }
    else
    {
        handleLockFailure("Could not acquire lock to verify temperature sensor state");
        return false;
    }

    // 2. Reset pinManager if sensor bad (PIDOUT domain)
    if (!tempOK)
    {
        if (appLockPidOut(LOCK_TIMEOUT_PIDOUT_MS))
        {
            g_app.pinManager.reset();
            appUnlockPidOut();
        }
        else
        {
            handleLockFailure("Could not acquire lock to reset pinManager");
        }
        return false;
    }

    return true;
}

// Only the Energy task calls this helper and adapter init/read. Once published,
// the adapter stays alive; read failures are recovered by the adapter itself.
static bool ensureEnergySource()
{
    if (g_activeSource.load()) return true;

    // Keep a successfully initialized adapter if publishing metadata must wait
    // for a lock. Do not repeat network initialization in that case.
    static std::unique_ptr<EnergySource> pending;
    if (!pending)
    {
        Setup setup;
        if (!appLockConfig(LOCK_TIMEOUT_CONFIG_MS)) return false;
        setup = g_app.webSockData.setupData;
        appUnlockConfig();

        auto attempt = [&](EnergySource* candidate) {
            std::unique_ptr<EnergySource> source(candidate);
            LOG_INFO(TAG_APP_SERVICES, "Energy task: initializing %s", source->name());
            if (source->init(setup, g_app.webSockData)) pending = std::move(source);
        };
#ifdef FRONIUS_IV
        attempt(new FroniusHTTP());
#endif
#ifdef AMIS_READER_DEV
        if (!pending) attempt(new AmisREST());
#endif
#ifdef HUAWEI_MODBUS
        if (!pending) attempt(new HuaweiModbus());
#endif
#ifdef LOXONE_API
        if (!pending) attempt(new LoxoneAsProvider());
#endif
        if (!pending) return false;
    }

    if (!appLockData(LOCK_TIMEOUT_DATA_MS)) return false;
    if (!appLockConfig(LOCK_TIMEOUT_CONFIG_MS))
    {
        appUnlockData();
        return false;
    }
    g_app.webSockData.setupData.akku = pending->hasBattery();
    g_app.webSockData.states.device = true;
    g_sourceType = pending->type();
    EnergySource* source = pending.release();
    g_activeSource.store(source);
    appUnlockConfig();
    appUnlockData();
    LOG_INFO(TAG_APP_SERVICES, "Energy source initialized: %s", source->name());
    return true;
}

static void markEnergyUnavailable(const char* reason)
{
    // A failed measurement says nothing about WLAN or MQTT connectivity.
    if (appLockData(LOCK_TIMEOUT_DATA_MS))
    {
        g_app.webSockData.energyState.valid = false;
        appUnlockData();
    }
    stopHeating(reason);
}

// -----------------------------------------------------------------------------
// fetchEnergy — unified: g_activeSource->read() -> energyState -> influx
//   g_activeSource ist eine EnergySource*-Pointer (energySource.h),
//   initialized and read exclusively by the Energy task.
// -----------------------------------------------------------------------------
static bool fetchEnergy()
{
    EnergySource* source = g_activeSource.load();
    if (!source)
    {
        LOG_ERROR(TAG_APP_SERVICES, "No active energy source");
        return false;
    } 

    LOG_DEBUG(TAG_APP_SERVICES, "Fetching energy data from %s...", source->name());

    if (!source->read(g_app.webSockData))
    {
        markEnergyUnavailable((String(source->name()) + " read failed").c_str());
        return false;
    }

    // energyState wird schon vom Adapter geloggt

#ifdef INFLUX
    influx_write(g_app.webSockData);
#endif

    return true;
}

// -----------------------------------------------------------------------------
// refreshEnergyDisplay – TFT refresh (TFT mutex + DATA domain)
// -----------------------------------------------------------------------------
static void refreshEnergyDisplay()
{
    if (xSemaphoreTake(g_tftMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        if (appLockData(LOCK_TIMEOUT_DATA_MS))
        {
            tft_drawInfo(g_app.webSockData);
            appUnlockData();
        }
        xSemaphoreGive(g_tftMutex);
    }
}
} // end namespace

// =============================================================================
// serviceWeather – Wetter-Vorhersage → preheatBonus für PID
//   Immediate first fetch; refresh within 6h, retry failures after 5min.
//   Last valid forecast is usable only until its expiry / local midnight.
// =============================================================================
#ifdef WEATHER_API
// Published/read under the DATA mutex. Network and JSON work stay outside it.
static PROGNOSE s_forecast;

void serviceWeather()
{
    static uint32_t lastAttempt = 0;
    static uint32_t retryAfter = 0;
    const uint32_t now = millis();
    if (retryAfter && now - lastAttempt < retryAfter) return;
    if (!networkIsAvailable() || time(nullptr) < 1704067200) return;
    lastAttempt = now;
    retryAfter = WEATHER_RETRY_INTERVAL_MS;
    PROGNOSE forecast;
    if (!weather_fetch(forecast)) return; // previous forecast expires normally
    if (appLockData(LOCK_TIMEOUT_DATA_MS)) {
        s_forecast = forecast;
        appUnlockData();
        const time_t lifetime = forecast.validUntil - time(nullptr);
        retryAfter = lifetime > 0 ? std::min<uint32_t>(WEATHER_FETCH_INTERVAL_MS,
            static_cast<uint32_t>(lifetime) * 1000) : WEATHER_RETRY_INTERVAL_MS;
    }
}

// Called by servicePid while DATA is locked; reset all features on expiry.
static PROGNOSE weatherSnapshot()
{
    const time_t now = time(nullptr);
    if (!s_forecast.valid || now < 1704067200 || now >= s_forecast.validUntil)
        return PROGNOSE{};
    return s_forecast;
}
#else
void serviceWeather() {}
static PROGNOSE weatherSnapshot() { return PROGNOSE{}; }
#endif

// =============================================================================
// serviceEnergy – Orchestrator
// =============================================================================
void serviceEnergy()
{
    if (isEnergyNight()) return;

    if (WiFi.status() != WL_CONNECTED)
    {
        tryMarkNetworkDown(
            "Energy service skipped because network is unavailable",
            "Energy service skipped, but could not acquire lock to update network state");
        return;
    }

    if (!checkTempSensorAndReset())
    {
        return;
    }

    if (!ensureEnergySource())
    {
        markEnergyUnavailable("Energy source initialization pending");
        return;
    }
    fetchEnergy();

    refreshEnergyDisplay();
} 

void servicePid()
{
    // KR-2: pinManager.update accesses both DATA (webSockData) + PIDOUT (pinManager)
    if (!appLockData(LOCK_TIMEOUT_DATA_MS)) return;
    if (!appLockPidOut(LOCK_TIMEOUT_PIDOUT_MS)) {
        appUnlockData();
        return;
    }
    {
    // Wetter-Bonus + TinyNN-Features
        const PROGNOSE forecast = weatherSnapshot();
        int bonus = forecast.preheatBonus;
        float pvRatio = forecast.pvRatio;
        float cloud = forecast.cloudAverage;
        float outTemp = forecast.tempOutside;
         
        g_app.pinManager.update(g_app.webSockData, bonus, pvRatio, cloud, outTemp, forecast.valid);
        appUnlockPidOut();
        appUnlockData();
    }
}

void serviceWeb()
{
    static uint32_t counter = 0;

    bool networkOK = false;
    // KR-2: states.networkOK is DATA domain
    if (appLockData(LOCK_TIMEOUT_DATA_MS))
    {
        networkOK = g_app.webSockData.states.networkOK;
        appUnlockData();
    }
    if (!networkOK)
    {
        return;
    }

    if (counter++ % 10 == 0)
    {
        cleanupClients();
        counter = 0;
    }
    notifyClients();

    if (appLockData(LOCK_TIMEOUT_DATA_MS))
    {
        bool changed = g_app.webSockData.setupData.setupChanged;
        appUnlockData();

        if (changed)
        {
            // KR-2: hotUpdate accesses DATA (webSockData) + PIDOUT (pinManager)
            if (appLockData(LOCK_TIMEOUT_MULTI_MS) && appLockPidOut(LOCK_TIMEOUT_MULTI_MS))
            {
                if (!hotUpdate(g_app.webSockData, g_app.pinManager))
                {
                    appUnlockPidOut();
                    appUnlockData();
                    LOG_DEBUG(TAG_APP_SERVICES, " Waiting for restart ....");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    esp_restart();
                    return;
                }
                else
                {
                    LOG_INFO(TAG_APP_SERVICES, "Hot update applied successfully without reboot");
                }
                g_app.webSockData.setupData.setupChanged = false;
                appUnlockPidOut();
                appUnlockData();
            }
            else
            {
                LOG_DEBUG(TAG_APP_SERVICES, "Setup changed, but could not acquire lock to apply hot update");
            }
        }
    }
}



void serviceMaintenance()
{
    serviceWeather(); // independent of energy polling, including at night
    g_app.heapSize[0].heapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    g_app.heapSize[0].heapSizeMax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

#ifdef CARD_READER
    {
        bool cardOK = false;
        // KR-2: states.cardWriterOK is DATA domain
        if (appLockData(LOCK_TIMEOUT_DATA_MS))
        {
            cardOK = g_app.webSockData.states.cardWriterOK;
            appUnlockData();
        }
        if (cardOK)
        {
            cardRW_flushLoggingFile();
            cardRW_closeLoggingFile();
        }
    }
#endif
}

void serviceEpromStore(void* param)
{
    Setup* setup = (Setup*)param;
    // er lokale Puffer, in den die Queue schreibt. Er wird dann in der Queue empfangen und in den Eprom geschrieben. So muss nicht die ganze Struktur in die Queue, sondern nur ein Zeiger auf den lokalen Puffer.
    LOG_INFO(TAG_APP_SERVICES, "app_services::serviceEpromStore - started");
    eprom_storeSetup(*setup);
}
