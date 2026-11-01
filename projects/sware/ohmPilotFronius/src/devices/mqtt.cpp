#include "mqtt.h"
#ifdef FEATURE_MQTT
#include "app_state.h"
#include "hotUpdate.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <freertos/queue.h>
#include <atomic>

// Only the clock task touches the client; producers enqueue bounded messages.
struct MqttMessage { char topic[64]; char payload[192]; };
static QueueHandle_t messages = nullptr;
static WiFiClient transport;
static PubSubClient client(transport);
static MQTT_SETUP config{};
static bool enabled = false;
static unsigned short port = 0;
static uint32_t lastAttempt = 0;
static bool attempted = false;
static std::atomic<uint32_t> applied{0};
uint32_t mqtt_appliedRevision() { return applied.load(); }

bool mqtt_init()
{
    if (!messages) messages = xQueueCreate(16, sizeof(MqttMessage));
    client.setSocketTimeout(2);
    return false; // Connections are established by mqtt_loop in its owning task.
}

bool mqtt_loop()
{
    MQTT_SETUP next;
    bool nextEnabled;
    unsigned short nextPort;
    uint32_t revision;
    if (!appLockConfig(50)) return client.connected();
    next = g_app.webSockData.setupData.mqttSetup;
    nextEnabled = g_app.webSockData.setupData.mqttEnabled;
    nextPort = g_app.webSockData.setupData.mqttPort;
    revision = setupAppliedRevision();
    appUnlockConfig();
    if (nextEnabled != enabled || nextPort != port || memcmp(&next, &config, sizeof(next))) {
        client.disconnect();
        config = next;
        enabled = nextEnabled;
        port = nextPort;
        attempted = false;
        if (messages) xQueueReset(messages);
        if (enabled) client.setServer(config.mqttHost, port);
    }
    applied.store(revision);
    if (!enabled || WiFi.status() != WL_CONNECTED) return false;
    const uint32_t now = millis();
    if (!client.connected() && (!attempted || uint32_t(now - lastAttempt) >= 5000)) {
        attempted = true;
        lastAttempt = now;
        client.connect("Energy-Harvester", config.mqttUser[0] ? config.mqttUser : nullptr, config.mqttPass);
    }
    if (client.connected()) {
        client.loop();
        MqttMessage message;
        for (unsigned i = 0; messages && i < 16 && xQueueReceive(messages, &message, 0) == pdTRUE; ++i)
            client.publish(message.topic, message.payload);
    }
    return client.connected();
}

static void publish(const char *topic, const char *payload)
{
    if (!messages || !payload) return;
    MqttMessage message{};
    strlcpy(message.topic, topic, sizeof(message.topic));
    strlcpy(message.payload, payload, sizeof(message.payload));
    xQueueSend(messages, &message, 0);
}
void mqtt_publish_en(int pwm, double availableWatt) {
    char value[48]; snprintf(value, sizeof(value), "%d", pwm); publish("energy/pwm", value);
    snprintf(value, sizeof(value), "%.2f", availableWatt); publish("energy/available", value);
}
void mqtt_publish_alarm_temp(int a, int b) {
    char value[32]; snprintf(value, sizeof(value), "%d", a); publish("alarm/sensor/s1", value);
    snprintf(value, sizeof(value), "%d", b); publish("alarm/sensor/s2", value);
}
void mqtt_publish_modbus_reconnect(const char *ip) { publish("modbus/connection/reconnect", ip); }
void mqtt_publish_modbus_wrong_production_val(double v) {
    char value[48]; snprintf(value, sizeof(value), "%.2f", v); publish("modbus/error/currentProduction", value);
}
void mqtt_publish_log(char token, const char *message) {
    const char *topic = token == 'W' ? "log/logw" : token == 'E' ? "log/loge" : token == 'D' ? "log/logd" : "log/logi";
    publish(topic, message);
}
#else
void mqtt_publish_log(char, const char *) {}
#endif
