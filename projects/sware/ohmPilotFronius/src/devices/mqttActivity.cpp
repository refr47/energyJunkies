#include "mqttActivity.h"

#ifdef FEATURE_MQTT
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <cstring>

namespace {
portMUX_TYPE activityMux = portMUX_INITIALIZER_UNLOCKED;
MqttActivityEvent activity[MQTT_ACTIVITY_CAPACITY]{};
unsigned nextSlot = 0;
unsigned count = 0;
uint32_t nextSequence = 0;
bool hasPublished = false;
bool hasReceived = false;
MqttActivityEvent lastPublished{};
MqttActivityEvent lastReceived{};
}

void mqttActivity_record(bool received, const char *topic, const char *payload)
{
    if (!topic || !payload) return;
    MqttActivityEvent event{};
    event.recordedAtMs = millis();
    event.received = received;
    strlcpy(event.topic, topic, sizeof(event.topic));
    strlcpy(event.payload, payload, sizeof(event.payload));

    portENTER_CRITICAL(&activityMux);
    event.sequence = ++nextSequence;
    if (received) {
        lastReceived = event;
        hasReceived = true;
    } else {
        lastPublished = event;
        hasPublished = true;
    }
    activity[nextSlot] = event;
    nextSlot = (nextSlot + 1) % MQTT_ACTIVITY_CAPACITY;
    if (count < MQTT_ACTIVITY_CAPACITY) ++count;
    portEXIT_CRITICAL(&activityMux);
}

void mqttActivity_clear()
{
    portENTER_CRITICAL(&activityMux);
    nextSlot = 0;
    count = 0;
    hasPublished = false;
    hasReceived = false;
    portEXIT_CRITICAL(&activityMux);
}

void mqttActivity_snapshot(MqttActivitySnapshot &snapshot)
{
    portENTER_CRITICAL(&activityMux);
    snapshot.count = count;
    snapshot.hasPublished = hasPublished;
    snapshot.hasReceived = hasReceived;
    if (hasPublished) snapshot.lastPublished = lastPublished;
    if (hasReceived) snapshot.lastReceived = lastReceived;
    for (unsigned i = 0; i < count; ++i)
        snapshot.events[i] = activity[(nextSlot + MQTT_ACTIVITY_CAPACITY - 1 - i) % MQTT_ACTIVITY_CAPACITY];
    portEXIT_CRITICAL(&activityMux);
}
#endif
