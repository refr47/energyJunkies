#include "app_services.h"

#include <Arduino.h>
#include <cstring>

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


#ifdef AMIS_READER_DEV
#include "amisReader.h"
#endif

#ifdef CARD_READER
#include "cardRW.h"
#endif

#include "modbusReader.h"

static constexpr uint32_t TEMPERATURE_OVERHEATED_WAIT_IN_SECS = 300;
static constexpr uint32_t NETWORK_RECOVERY_GRACE_CYCLES = 2;
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
    if (!appLockData(10) || !appLockPidOut(10))
    {
        if (appLockData(10)) appUnlockData();
        if (appLockPidOut(10)) appUnlockPidOut();
        LOG_DEBUG(TAG_APP_SERVICES, "stopHeating: could not acquire locks for: %s", reason);
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
    LOG_INFO(TAG_APP_SERVICES, "app_services::serviceClock - update time and ip addr");

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
    // char *ipBuffer = g_app.webSockData.setupData.currentIP;
    String s = WiFi.localIP().toString();
    LOG_INFO(TAG_APP_SERVICES, "Current IP: %s", s.c_str());
    ledHandler_blink();

#ifdef MQTT
    {
        // KR-2: states.mqtt is DATA domain
        bool mqttActive = false;
        if (appLockData(10))
        {
            mqttActive = g_app.webSockData.states.mqtt;
            appUnlockData();
        }
        if (mqttActive)
        {
            mqtt_loop();
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
    static uint32_t recoveryCycles = 0;

    LOG_INFO(TAG_APP_SERVICES, "app_services::serviceNetworkSupervisor ");

    // -- Netzwerkpruefung: setupData unter Lock kopieren --
    Setup localSetup;
    {
        // KR-2: setupData is CONFIG domain
        if (appLockConfig(10))
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

    // KR-2: states.networkOK/mqtt are DATA domain
    if (appLockData(10))
    {
        g_app.webSockData.states.networkOK = true;
#ifdef MQTT
        g_app.webSockData.states.mqtt = true;
#endif
        appUnlockData();
    }

    ledHandler_showNetworkError(false);

#ifdef FRONIUS_IV
    {
        bool modbusOK = true;
        // KR-2: states.modbusOK is DATA domain, mb_init needs setupData (also DATA)
        if (appLockData(10))
        {
            modbusOK = g_app.webSockData.states.modbusOK;
            appUnlockData();
        }
        if (!modbusOK)
        {
            LOG_ERROR(TAG_APP_SERVICES, "Modbus connection to Fronius failed, try reconnecting");
            if (appLockData(10))
            {
                g_app.webSockData.states.modbusOK = mb_init(g_app.webSockData.setupData);
                appUnlockData();
            }
            else
            {
                LOG_DEBUG(TAG_APP_SERVICES, "Modbus connection seems down, but could not acquire lock to update state");
            }
        }
    }
#endif
#ifdef AMIS_READER_DEV
    {
        bool amisOK = false;
        // KR-2: states.amisReader is DATA domain
        if (appLockData(10))
        {
            amisOK = g_app.webSockData.states.amisReader;
            appUnlockData();
        }
        if (!amisOK)
        {
            LOG_ERROR(TAG_APP_SERVICES, "AmisReader connection failed, try reconnecting");
            if (appLockData(10))
            {
                g_app.webSockData.states.amisReader = amisReader_initRestTargets(g_app.webSockData);
                appUnlockData();
            }
            else
            {
                LOG_DEBUG(TAG_APP_SERVICES, "AmisReader connection seems down, but could not acquire lock to update state");
            }
        }
    }
#endif
}

void serviceTemperature()
{
    LOG_INFO(TAG_APP_SERVICES, "app_services::serviceTemperature ");
    if (!temp_getTemperature(g_app.webSockData.temperature))
    {
        ledHandler_showTemperaturError(true);
        // KR-2: this section accesses states/temperature (DATA) + pinManager/alarmContainer (PIDOUT)
        if (appLockData(10) && appLockPidOut(10))
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
    if (appLockData(10))
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
    if (appLockPidOut(10))
    {
        g_app.pinManager.reset();
        appUnlockPidOut();
    }
    LOG_DEBUG(TAG_APP_SERVICES, "%s", context);
}

// --- Helper: mark network down with lock, fail-safe otherwise ---
static void tryMarkNetworkDown(const char* reason, const char* failMsg)
{
    // KR-2: needs Data + PidOut for markNetworkDown
    if (!appLockData(10) || !appLockPidOut(10))
    {
        g_app.pinManager.reset();
        LOG_DEBUG(TAG_APP_SERVICES, "%s", failMsg);
        return;
    }

    markNetworkDown(reason);
    appUnlockPidOut();
    appUnlockData();
}


void serviceEnergy()
{
    LOG_INFO(TAG_APP_SERVICES, "app_services::serviceEnergy - ");
    if (!networkIsAvailable())
    {
        tryMarkNetworkDown("Energy service skipped because network is unavailable",
            "Energy service skipped, but could not acquire lock to update network state");
        return;
    }

    // -- tempOK check unter Lock --
    {
        bool tempOK = true;
        // KR-2: states.tempSensorOK is DATA domain
        if (appLockData(10))
        {
            tempOK = g_app.webSockData.states.tempSensorOK;
            appUnlockData();
        }
        else
        {
            handleLockFailure("Could not acquire lock to verify temperature sensor state");
            return;
        }

        if (!tempOK)
        {
            g_app.pinManager.reset();
            return;
        }
    }

    // -- Energie-Quelle unter Lock ermitteln --
    enum EnergySource { SRC_NONE, SRC_FRONIUS, SRC_AMIS };
    EnergySource energySource = SRC_NONE;
    {
        // KR-2: states fields are DATA domain
        if (appLockData(10))
        {
#ifdef FRONIUS_IV
            if (g_app.webSockData.states.froniusAPI && g_app.webSockData.states.networkOK)
                energySource = SRC_FRONIUS;
#endif

#ifdef AMIS_READER_DEV
            if (g_app.webSockData.states.amisReader && g_app.webSockData.states.networkOK)
                energySource = SRC_AMIS;

#endif
            appUnlockData();
        }
        else
        {
            handleLockFailure("Could not acquire lock to select energy source");
        }
    }


    // -- I/O und Cache-Update je nach Quelle --
    if (energySource == SRC_FRONIUS)
    {

        if (!solar_get_powerflow(g_app.webSockData))
        {
            tryMarkNetworkDown("Fronius API read failed",
                "Fronius API failed, but could not acquire lock to update state");
            return;
        }

        if (appLockData(10))
        {
            g_app.webSockData.mbContainer.akkuStr.data.chargeRate = g_app.webSockData.fronius_SOLAR_POWERFLOW.p_akku;
            g_app.webSockData.mbContainer.akkuStr.data.dischargeRate = g_app.webSockData.fronius_SOLAR_POWERFLOW.rel_Autonomy;
            g_app.webSockData.mbContainer.akkuStr.data.maxChargeRate = g_app.webSockData.fronius_SOLAR_POWERFLOW.rel_SelfConsumption;

            g_app.webSockData.mbContainer.inverterSumValues.data.acCurrentPower =
                g_app.webSockData.fronius_SOLAR_POWERFLOW.p_akku +
                g_app.webSockData.fronius_SOLAR_POWERFLOW.p_pv;
            g_app.webSockData.mbContainer.meterValues.data.acCurrentPower =
                g_app.webSockData.fronius_SOLAR_POWERFLOW.p_load;
            appUnlockData();
        }
        else
        {
            handleLockFailure("Fronius API succeeded, but could not acquire lock to cache results");
        }

        if (!mb_readInverter(g_app.webSockData.setupData, g_app.webSockData.mbContainer))
        {
            tryMarkNetworkDown("Modbus read failed",
                "Modbus read failed, but could not acquire lock to update state");
            return;
        }

        if (appLockData(10))
        {
            g_app.webSockData.pidContainer.mCurrentPower =
                g_app.webSockData.mbContainer.meterValues.data.acCurrentPower;
            appUnlockData();
        }
        else
        {
            handleLockFailure("Modbus read succeeded, but could not acquire lock to update PID power");
        }

#ifdef INFLUX
        influx_write(g_app.webSockData);
#endif

    }
    else if (energySource == SRC_AMIS)
    {

        if (!amisReader_readRestTarget(g_app.webSockData))
        {
            tryMarkNetworkDown("AMIS reader failed",
                "AMIS reader failed, but could not acquire lock to update state");
            return;
        }

        if (appLockData(10))
        {
            g_app.webSockData.mbContainer.inverterSumValues.data.acCurrentPower =
                g_app.webSockData.amisReader.exportInWatt;
            g_app.webSockData.mbContainer.inverterSumValues.data.acTotalEnergy = 0.0;
            g_app.webSockData.mbContainer.inverterSumValues.data.dcCurrentPower = 0.0;
            g_app.webSockData.mbContainer.meterValues.data.acTotalEnergyExp =
                g_app.webSockData.amisReader.absolutExportInkWh;
            g_app.webSockData.mbContainer.meterValues.data.acCurrentPower =
                g_app.webSockData.amisReader.saldo;
            appUnlockData();
        }  
        else 
        {
            handleLockFailure("AMIS reader succeeded, but could not acquire lock to update state");
        }
    }

    // -- Display aktualisieren unter Lock --
    if (xSemaphoreTake(g_tftMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        // KR-2: webSockData is DATA domain
        if (appLockData(10))
        {
            tft_drawInfo(g_app.webSockData);
            appUnlockData();
        }
        xSemaphoreGive(g_tftMutex);
    }
}

void servicePid()
{
    LOG_INFO(TAG_APP_SERVICES, "app_services::servicePid - ");
    // KR-2: pinManager.update accesses both DATA (webSockData) + PIDOUT (pinManager)
    if (appLockData(10) && appLockPidOut(10))
    {
        g_app.pinManager.update(g_app.webSockData);
        appUnlockPidOut();
        appUnlockData();
    }
}

void serviceWeb()
{
    static uint32_t counter = 0;

    bool networkOK = false;
    // KR-2: states.networkOK is DATA domain
    if (appLockData(10))
    {
        networkOK = g_app.webSockData.states.networkOK;
        appUnlockData();
    }
    if (!networkOK)
    {
        LOG_INFO(TAG_APP_SERVICES, "ServiceWeb cannot be executed due to network down state");
        return;
    }

    if (counter++ % 10 == 0)
    {
        cleanupClients();
        counter = 0;
    }
    notifyClients();

    if (appLockData(10))
    {
        bool changed = g_app.webSockData.setupData.setupChanged;
        appUnlockData();

        LOG_INFO(TAG_APP_SERVICES, "ServiceWeb - data changed? %d", changed);
        if (changed)
        {
            // KR-2: hotUpdate accesses DATA (webSockData) + PIDOUT (pinManager)
            if (appLockData(100) && appLockPidOut(100))
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
    LOG_INFO(TAG_APP_SERVICES, "app_services::serviceMaintenance - ");
    g_app.heapSize[0].heapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    g_app.heapSize[0].heapSizeMax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

#ifdef CARD_READER
    {
        bool cardOK = false;
        // KR-2: states.cardWriterOK is DATA domain
        if (appLockData(10))
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
