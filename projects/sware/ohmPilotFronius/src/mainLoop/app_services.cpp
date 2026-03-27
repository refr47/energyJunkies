#include "app_services.h"

#include <Arduino.h>
#include <cstring>

#include "app_state.h"
#include "app_sync.h"

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

#ifdef webSockData.fronius_SOLAR_POWERFLOW_IV
#include "froniusSolarAPI.h"
#endif

#ifdef AMIS_READER_DEV
#include "amisReader.h"
#endif

#ifdef CARD_READER
#include "cardRW.h"
#endif

#include "modbusReader.h"

static constexpr uint32_t TEMPERATURE_OVERHEATED_WAIT_IN_SECS = 300;

static float averageTemp()
{
    return (g_app.webSockData.temperature.sensor1 + g_app.webSockData.temperature.sensor2) / 2.0f;
}

void serviceClock()
{
    appLock();
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

#ifdef MQTT
    if (g_app.webSockData.states.mqtt)
    {
        mqtt_loop();
    }
#endif

    appUnlock();
}

void serviceNetworkSupervisor()
{
    appLock();

    if (!wifi_isStillConnected(g_app.webSockData.setupData))
    {
        LOG_ERROR("Network down, try reconnecting");
        g_app.webSockData.states.networkOK = false;
        g_app.webSockData.states.mqtt = false;
        WiFi.disconnect();
    }
    else
    {
        int rssi = 0;
        esp_err_t result = esp_wifi_sta_get_rssi(&rssi);
        if (result == ESP_OK)
        {
            long saldo = 0;
            char *amisReaderHost = NULL;
            if (g_app.webSockData.states.amisReader) {
                amisReaderHost = g_app.webSockData.setupData.amisReaderHost;
                saldo = g_app.webSockData.amisReader.saldo;
            } else {
                amisReaderHost = "0.0.0.0";
            }

            LOG_INFO("Network OK, SSID: %s, IP: %s, RSSI: %d, AmisReader: %s Saldo: %ld",
                     g_app.webSockData.setupData.ssid,
                     g_app.webSockData.setupData.currentIP,
                     rssi,
                     amisReaderHost,
                     saldo);
        }
        g_app.webSockData.states.networkOK = true;
#ifdef MQTT
        g_app.webSockData.states.mqtt = true;
#endif
    }

    appUnlock();
}

void serviceTemperature()
{
    appLock();

    if (!temp_getTemperature(g_app.webSockData.temperature))
    {
        g_app.webSockData.states.tempUnderflow = false;
        g_app.webSockData.states.tempSensorOK = false;

        if (g_app.webSockData.temperature.sensor1 < 0.0f &&
            g_app.webSockData.temperature.sensor2 < 0.0f)
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
                LOG_ERROR("Temperature sensors failed - heater off");
                g_app.pinManager.reset();
                g_app.alarmContainer.alarmTemp.alarmTemp = true;
                g_app.alarmContainer.alarmTemp.overFlowHappenedAt = time_getTimeStamp();
                g_app.webSockData.temperature.alarm = true;
            }
        }

        appUnlock();
        return;
    }

    g_app.webSockData.states.tempSensorOK = true;

    const float tempAvg = averageTemp();

    if (tempAvg < g_app.webSockData.setupData.tempMinInGrad)
    {
        g_app.webSockData.states.tempUnderflow = true;
        g_app.pinManager.allOn();
        LOG_INFO("Temp under minimum, heating full power");
        appUnlock();
        return;
    }

    g_app.webSockData.states.tempUnderflow = false;

    if (tempAvg < g_app.webSockData.setupData.tempMaxAllowedInGrad)
    {
        g_app.webSockData.temperature.alarm = false;
    }

    if (!g_app.alarmContainer.alarmTemp.alarmTemp)
    {
        if (tempAvg > g_app.webSockData.setupData.tempMaxAllowedInGrad)
        {
            LOG_ERROR("Temperature limit reached - heater off");
            g_app.pinManager.reset();
            g_app.alarmContainer.alarmTemp.alarmTemp = true;
            g_app.alarmContainer.alarmTemp.overFlowHappenedAt = time_getTimeStamp();
            g_app.webSockData.temperature.alarm = true;
            ledHandler_showTemperaturError(true);

#ifdef MQTT
            if (g_app.webSockData.states.mqtt)
            {
                mqtt_publish_alarm_temp(g_app.webSockData.temperature.sensor1,
                                        g_app.webSockData.temperature.sensor2);
            }
#endif
        }
    }
    else
    {
        const time_t currT = time_getTimeStamp();
        const double diffT = difftime(currT, g_app.alarmContainer.alarmTemp.overFlowHappenedAt);

        if (diffT > TEMPERATURE_OVERHEATED_WAIT_IN_SECS)
        {
            if (tempAvg > g_app.webSockData.setupData.tempMaxAllowedInGrad)
            {
                g_app.alarmContainer.alarmTemp.overFlowHappenedAt = currT;
                ledHandler_showTemperaturError(true);
#ifdef MQTT
                if (g_app.webSockData.states.mqtt)
                {
                    mqtt_publish_alarm_temp(g_app.webSockData.temperature.sensor1,
                                            g_app.webSockData.temperature.sensor2);
                }
#endif
            }
            else
            {
                g_app.alarmContainer.alarmTemp.alarmTemp = false;
                g_app.alarmContainer.alarmTemp.overFlowHappenedAt = 0;
                ledHandler_showTemperaturError(false);
                LOG_INFO("Temperature alarm reset");
            }
        }
    }

    appUnlock();
}

void serviceEnergy()
{
    appLock();

    if (!g_app.webSockData.states.tempSensorOK)
    {
        appUnlock();
        return;
    }

#ifdef FRONIUS_IV
    if (g_app.webSockData.states.froniusAPI && g_app.webSockData.states.networkOK)
    {
        if (solar_get_powerflow(g_app.webSockData))
        {
            g_app.webSockData.mbContainer.akkuStr.data.chargeRate = g_app.webSockData.fronius_SOLAR_POWERFLOW.p_akku;
            g_app.webSockData.mbContainer.akkuStr.data.dischargeRate = g_app.webSockData.fronius_SOLAR_POWERFLOW.rel_Autonomy;
            g_app.webSockData.mbContainer.akkuStr.data.maxChargeRate = g_app.webSockData.fronius_SOLAR_POWERFLOW.rel_SelfConsumption;

            g_app.webSockData.mbContainer.inverterSumValues.data.acCurrentPower = g_app.webSockData.fronius_SOLAR_POWERFLOW.p_akku +
                                                                                  g_app.webSockData.fronius_SOLAR_POWERFLOW.p_pv;
            g_app.webSockData.mbContainer.meterValues.data.acCurrentPower = g_app.webSockData.fronius_SOLAR_POWERFLOW.p_load;
        }
        else
        {
            g_app.webSockData.states.networkOK = false;
            appUnlock();
            return;
        }
    }


    if (!g_app.webSockData.states.froniusAPI &&
        g_app.webSockData.states.modbusOK &&
        g_app.webSockData.states.networkOK)
    {
        if (mb_readInverter(g_app.webSockData.setupData, g_app.webSockData.mbContainer))
        {
            g_app.webSockData.pidContainer.mCurrentPower = g_app.webSockData.mbContainer.meterValues.data.acCurrentPower;

#ifdef INFLUX
            influx_write(g_app.webSockData);
#endif
        }
        else
        {
            LOG_ERROR("Modbus read failed");
            g_app.webSockData.states.networkOK = false;
        }
    }
#endif // #elif FRONIUS_IV

#ifdef AMIS_READER_DEV
    if (g_app.webSockData.states.amisReader && g_app.webSockData.states.networkOK)
    {
        if (amisReader_readRestTarget(g_app.webSockData))
        {
            g_app.webSockData.mbContainer.inverterSumValues.data.acCurrentPower = g_app.webSockData.amisReader.exportInWatt;
            g_app.webSockData.mbContainer.inverterSumValues.data.acTotalEnergy = 0.0;
            g_app.webSockData.mbContainer.inverterSumValues.data.dcCurrentPower = 0.0;
            g_app.webSockData.mbContainer.meterValues.data.acTotalEnergyExp = g_app.webSockData.amisReader.absolutExportInkWh;
            g_app.webSockData.mbContainer.meterValues.data.acCurrentPower = g_app.webSockData.amisReader.saldo;
            LOG_INFO("AMIS reader OK, Saldo: %ld, ", g_app.webSockData.amisReader.saldo);
        }
        else
        {
            LOG_ERROR("AMIS reader failed");
            g_app.webSockData.states.networkOK = false;
        }
    }
#endif

    if (xSemaphoreTake(g_tftMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        tft_drawInfo(g_app.webSockData);
        xSemaphoreGive(g_tftMutex);
    }

    appUnlock();
}

void servicePid()
{
    appLock();

    if (g_app.webSockData.states.networkOK)
    {
        if (!g_app.alarmContainer.alarmTemp.alarmTemp)
        {
            g_app.pinManager.update(g_app.webSockData);
            g_app.webSockData.pidContainer.mAnalogOut = g_app.pinManager.getStateOfAnaPin();
            g_app.webSockData.pidContainer.PID_PIN1 = g_app.pinManager.getStateOfDigPin(0);
            g_app.webSockData.pidContainer.PID_PIN2 = g_app.pinManager.getStateOfDigPin(1);
        }
        else
        {
            g_app.pinManager.reset();
        }
    }

    appUnlock();
}

void serviceWeb()
{
    appLock();
    notifyClients(getJsonObj());

    if (g_app.webSockData.setupData.setupChanged)
    {
        if (!hotUpdate(g_app.webSockData, g_app.pinManager))
        {
            appUnlock();
            delay(1000);
            esp_restart();
            return;
        }
        g_app.webSockData.setupData.setupChanged = false;
    }
    appUnlock();
}

void serviceMaintenance()
{
    appLock();

    g_app.heapSize[0].heapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    g_app.heapSize[0].heapSizeMax = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

#ifdef CARD_READER
    if (g_app.webSockData.states.cardWriterOK)
    {
        cardRW_flushLoggingFile();
        cardRW_closeLoggingFile();
    }
#endif

    logReader_init();

    appUnlock();
}