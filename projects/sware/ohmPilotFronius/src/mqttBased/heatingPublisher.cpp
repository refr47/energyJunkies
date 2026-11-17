#include "heatingPublisher.h"

#if defined(FEATURE_MQTT) && defined(LOXONE_API)
#include "app_state.h"
#include "debugConsole.h"
#include "mqtt.h"
#include "utils.h"
#include <cstring>

namespace {
constexpr char POWER_TOPIC[] = "boiler/watt";
constexpr char TEMPERATURE_TOPIC[] = "boiler/temp";
constexpr uint32_t REFRESH_MS = 15U * 60U * 1000U;

// All state is protected by the application data lock. The change handlers
// are called under that lock; resendSnapshot takes it itself.
bool havePower = false;
bool haveTemperature = false;
int lastPower = 0;
int lastTemperature = 0;
uint32_t lastPowerAt = 0;
uint32_t lastTemperatureAt = 0;

bool enqueueValue(const char *topic, int value)
{
    char payload[16];
    snprintf(payload, sizeof(payload), "%d", value);
    return mqtt_publish_retained(topic, payload);
}

bool shouldPublish(bool haveValue, int value, int lastValue,
                   uint32_t lastAt, uint32_t now)
{
    return !haveValue || value != lastValue || uint32_t(now - lastAt) >= REFRESH_MS;
}
}

void heatingPublisher_powerChanged(int watts)
{
    if (isEnergyNight(g_app.webSockData.setupData.energyPolling)) {
        havePower = false;
        return;
    }
    const uint32_t now = millis();
    if (shouldPublish(havePower, watts, lastPower, lastPowerAt, now) &&
        enqueueValue(POWER_TOPIC, watts)) {
        LOG_INFO(TAG_MQTT,
                 "Heater estimated power queued: %d W (relay phases=%d PWM=%d/255 rated=%u W)",
                 watts, int(g_app.webSockData.pidContainer.PID_PIN1) +
                        int(g_app.webSockData.pidContainer.PID_PIN2),
                 g_app.webSockData.pidContainer.mAnalogOut,
                 g_app.webSockData.setupData.heizstab_leistung_in_watt);
        lastPower = watts;
        lastPowerAt = now;
        havePower = true;
    }
}

void heatingPublisher_temperatureChanged(int degrees)
{
    if (isEnergyNight(g_app.webSockData.setupData.energyPolling)) {
        haveTemperature = false;
        return;
    }
    const uint32_t now = millis();
    if (shouldPublish(haveTemperature, degrees, lastTemperature, lastTemperatureAt, now) &&
        enqueueValue(TEMPERATURE_TOPIC, degrees)) {
        lastTemperature = degrees;
        lastTemperatureAt = now;
        haveTemperature = true;
    }
}

bool heatingPublisher_isTopic(const char *topic)
{
    return topic && (!strcmp(topic, POWER_TOPIC) || !strcmp(topic, TEMPERATURE_TOPIC));
}

bool heatingPublisher_isNightRest()
{
    if (!appLockData(50)) return true;
    const EnergyPollingConfig config = g_app.webSockData.setupData.energyPolling;
    appUnlockData();
    return isEnergyNight(config);
}

bool heatingPublisher_resendSnapshot()
{
    if (!appLockData(50)) return false;
    if (isEnergyNight(g_app.webSockData.setupData.energyPolling)) {
        havePower = false;
        haveTemperature = false;
        appUnlockData();
        return true;
    }
    const int watts = static_cast<int>(g_app.webSockData.pidContainer.mCurrentPower);
    const bool temperatureValid = g_app.webSockData.states.tempSensorOK;
    const int degrees = (g_app.webSockData.temperature.sensor1 +
                         g_app.webSockData.temperature.sensor2) / 2;
    const uint32_t now = millis();
    const bool powerQueued = enqueueValue(POWER_TOPIC, watts);
    const bool temperatureQueued = !temperatureValid || enqueueValue(TEMPERATURE_TOPIC, degrees);
    if (powerQueued) {
        lastPower = watts;
        lastPowerAt = now;
        havePower = true;
    }
    if (temperatureValid && temperatureQueued) {
        lastTemperature = degrees;
        lastTemperatureAt = now;
        haveTemperature = true;
    }
    appUnlockData();
    return powerQueued && temperatureQueued;
}
#endif
