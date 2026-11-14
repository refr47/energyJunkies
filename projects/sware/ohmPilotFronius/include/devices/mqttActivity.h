#pragma once

#include <stdint.h>

#ifdef FEATURE_MQTT

constexpr unsigned MQTT_ACTIVITY_CAPACITY = 8;

struct MqttActivityEvent {
    uint32_t sequence;
    uint32_t recordedAtMs;
    bool received;
    char topic[49];
    char payload[33];
};

struct MqttActivitySnapshot {
    MqttActivityEvent events[MQTT_ACTIVITY_CAPACITY];
    unsigned count;
    bool hasPublished;
    bool hasReceived;
    MqttActivityEvent lastPublished;
    MqttActivityEvent lastReceived;
};

// Both MQTT clients record into this bounded, task-safe buffer.
void mqttActivity_record(bool received, const char *topic, const char *payload);
void mqttActivity_clear();
void mqttActivity_snapshot(MqttActivitySnapshot &snapshot);

#endif
