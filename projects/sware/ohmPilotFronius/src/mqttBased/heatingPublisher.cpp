#include "heatingPublisher.h"

#if defined(FEATURE_MQTT) && defined(LOXONE_API)
#include "app_state.h"
#include "mqtt.h"
#include <cstring>

namespace {
constexpr char POWER_TOPIC[] = "heizung/consum_watt";
constexpr char TEMPERATURE_TOPIC[] = "heizung/temp";
bool havePower = false;
bool haveTemperature = false;
int lastPower = 0;
int lastTemperature = 0;

bool enqueueValue(const char *topic, int value)
{
    char payload[16];
    snprintf(payload, sizeof(payload), "%d", value);
    return mqtt_publish_retained(topic, payload);
}
}

void heatingPublisher_powerChanged(int watts)
{
    if ((!havePower || watts != lastPower) && enqueueValue(POWER_TOPIC, watts)) {
        lastPower = watts;
        havePower = true;
    }
}

void heatingPublisher_temperatureChanged(int degrees)
{
    if ((!haveTemperature || degrees != lastTemperature) && enqueueValue(TEMPERATURE_TOPIC, degrees)) {
        lastTemperature = degrees;
        haveTemperature = true;
    }
}

bool heatingPublisher_isTopic(const char *topic)
{
    return topic && (!strcmp(topic, POWER_TOPIC) || !strcmp(topic, TEMPERATURE_TOPIC));
}

bool heatingPublisher_resendSnapshot()
{
    if (!appLockData(50)) return false;
    const int watts = static_cast<int>(g_app.webSockData.pidContainer.mCurrentPower);
    const bool temperatureValid = g_app.webSockData.states.tempSensorOK;
    const int degrees = (g_app.webSockData.temperature.sensor1 +
                         g_app.webSockData.temperature.sensor2) / 2;
    const bool powerQueued = enqueueValue(POWER_TOPIC, watts);
    const bool temperatureQueued = !temperatureValid || enqueueValue(TEMPERATURE_TOPIC, degrees);
    appUnlockData();
    return powerQueued && temperatureQueued;
}
#endif
