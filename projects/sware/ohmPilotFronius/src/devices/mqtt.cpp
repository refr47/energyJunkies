#include "mqtt.h"
#ifdef FEATURE_MQTT
#include "app_state.h"
#include "hotUpdate.h"
#include "mqttActivity.h"
#include "debugConsole.h"
#include "gridLoads.h"
#ifdef LOXONE_API
#include "heatingPublisher.h"
#endif
#include <PubSubClient.h>
#include <WiFi.h>
#include <freertos/queue.h>
#include <atomic>

// Only the clock task touches the client; producers enqueue bounded messages.
struct MqttMessage { char topic[64]; char payload[192]; bool retained; uint32_t queuedAt; };
static QueueHandle_t messages = nullptr;
static WiFiClient transport;
static PubSubClient client(transport);
static MQTT_SETUP config{};
static bool enabled = false;
static unsigned short port = 0;
static uint32_t lastAttempt = 0;
static bool attempted = false;
static const char *const *receiverTopics = nullptr;
static unsigned receiverTopicCount = 0;
static MqttMessageHandler receiverMessage = nullptr;
static MqttConnectionHandler receiverConnection = nullptr;
static std::atomic<void *> receiverContext{nullptr};
static const char *const *loadTopics = nullptr;
static unsigned loadTopicCount = 0;
static MqttMessageHandler loadMessage = nullptr;
static MqttConnectionHandler loadConnection = nullptr;
static std::atomic<void *> loadContext{nullptr};
// Subscription state belongs to the MQTT task. The two receivers can register
// at different times, including after the broker connection is established.
static bool energySubscribed = false;
static bool loadSubscribed = false;
#ifdef LOXONE_API
static bool heatingSnapshotPending = false;
#endif
static std::atomic<uint32_t> applied{0};
uint32_t mqtt_appliedRevision() { return applied.load(); }

bool mqtt_init()
{
    if (!messages) messages = xQueueCreate(16, sizeof(MqttMessage));
    client.setSocketTimeout(2);
    client.setCallback([](char *topic, uint8_t *payload, unsigned int length) {
        LOG_INFO(TAG_MQTT, "MQTT received topic=%s payload=%.*s length=%u",
                 topic, static_cast<int>(length < 48 ? length : 48),
                 reinterpret_cast<const char *>(payload), length);
        void *context = receiverContext.load();
        if (context && receiverMessage)
            for (unsigned i = 0; i < receiverTopicCount; ++i)
                if (!strcmp(topic, receiverTopics[i])) {
                    receiverMessage(context, topic, payload, length);
                    break;
                }
        context = loadContext.load();
        if (context && loadMessage)
            for (unsigned i = 0; i < loadTopicCount; ++i)
                if (!strcmp(topic, loadTopics[i])) {
                    loadMessage(context, topic, payload, length);
                    break;
                }
    });
    return false; // Connections are established by mqtt_loop in its owning task.
}

bool mqtt_register_load_receiver(const char *const *topics, unsigned count, void *context,
                                 MqttMessageHandler onMessage, MqttConnectionHandler onConnection)
{
    if (!topics || !count || !context || !onMessage || !onConnection || loadContext.load())
        return false;
    loadTopics = topics;
    loadTopicCount = count;
    loadMessage = onMessage;
    loadConnection = onConnection;
    loadContext.store(context);
    return true;
}

bool mqtt_register_receiver(const char *const *topics, unsigned count, void *context,
                            MqttMessageHandler onMessage, MqttConnectionHandler onConnection)
{
    if (!topics || !count || !context || !onMessage || !onConnection || receiverContext.load())
        return false;
    receiverTopics = topics;
    receiverTopicCount = count;
    receiverMessage = onMessage;
    receiverConnection = onConnection;
    receiverContext.store(context);
    return true;
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
        if (receiverContext.load())
            receiverConnection(receiverContext.load(), false, true);
        if (loadContext.load())
            loadConnection(loadContext.load(), false, true);
        energySubscribed = false;
        loadSubscribed = false;
        mqttActivity_clear();
        config = next;
        enabled = nextEnabled;
        port = nextPort;
        attempted = false;
        if (messages) xQueueReset(messages);
        if (enabled) client.setServer(config.mqttHost, port);
    }
    applied.store(revision);
    if (!enabled || WiFi.status() != WL_CONNECTED) {
        if (energySubscribed && receiverContext.load())
            receiverConnection(receiverContext.load(), false, false);
        if (loadSubscribed && loadContext.load())
            loadConnection(loadContext.load(), false, false);
        energySubscribed = false;
        loadSubscribed = false;
        return false;
    }
    const uint32_t now = millis();
#ifdef LOXONE_API
    bool justConnected = false;
#endif
    if (!client.connected() && (!attempted || uint32_t(now - lastAttempt) >= 5000)) {
        attempted = true;
        lastAttempt = now;
#ifdef LOXONE_API
        justConnected = client.connect("Energy-Harvester", config.mqttUser[0] ? config.mqttUser : nullptr, config.mqttPass);
        if (justConnected) heatingSnapshotPending = true;
#else
        client.connect("Energy-Harvester", config.mqttUser[0] ? config.mqttUser : nullptr, config.mqttPass);
#endif
    }
    if (!client.connected()) {
        if (energySubscribed && receiverContext.load()) receiverConnection(receiverContext.load(), false, false);
        if (loadSubscribed && loadContext.load()) loadConnection(loadContext.load(), false, false);
        energySubscribed = false;
        loadSubscribed = false;
    }
    if (client.connected()) {
        void *context = receiverContext.load();
        if (context && !energySubscribed) {
            bool subscribed = true;
            for (unsigned i = 0; i < receiverTopicCount; ++i) {
                if (!client.subscribe(receiverTopics[i])) {
                    LOG_ERROR(TAG_MQTT, "MQTT subscribe failed: %s", receiverTopics[i]);
                    subscribed = false;
                } else LOG_INFO(TAG_MQTT, "MQTT subscribed: %s", receiverTopics[i]);
            }
            if (subscribed) {
                energySubscribed = true;
                receiverConnection(context, true, false);
            }
        }
        void *loadReceiver = loadContext.load();
        if (loadReceiver && !loadSubscribed) {
            bool subscribed = true;
            for (unsigned i = 0; i < loadTopicCount; ++i) {
                if (!client.subscribe(loadTopics[i])) {
                    LOG_ERROR(TAG_MQTT, "MQTT subscribe failed: %s", loadTopics[i]);
                    subscribed = false;
                }
            }
            if (subscribed) {
                loadSubscribed = true;
                loadConnection(loadReceiver, true, false);
            }
        }
        client.loop();
        if (!client.connected()) {
            if (energySubscribed && context) receiverConnection(context, false, false);
            if (loadSubscribed && loadReceiver) loadConnection(loadReceiver, false, false);
            energySubscribed = false;
            loadSubscribed = false;
            return false;
        }
        MqttMessage message;
#ifdef LOXONE_API
        const bool nightRest = heatingPublisher_isNightRest();
#endif
        for (unsigned i = 0; messages && i < 16 && xQueueReceive(messages, &message, 0) == pdTRUE; ++i) {
            // A delayed OFF/ON command must not be applied after its decision expired.
            if (!strncmp(message.topic, "energy/loads/", 13) &&
                uint32_t(millis() - message.queuedAt) > 10000) continue;
            if (!strncmp(message.topic, "energy/loads/", 13) &&
                !gridLoadsCommandCurrent(message.topic, message.payload)) continue;
#ifdef LOXONE_API
            if (heatingPublisher_isTopic(message.topic) &&
                (justConnected || nightRest)) continue;
#endif
            if (client.publish(message.topic, message.payload, message.retained))
                mqttActivity_record(false, message.topic, message.payload);
        }
#ifdef LOXONE_API
        if (heatingSnapshotPending) {
            // Replace offline heater transitions with the current retained state.
            heatingSnapshotPending = !heatingPublisher_resendSnapshot();
            for (unsigned i = 0; messages && i < 2 && xQueueReceive(messages, &message, 0) == pdTRUE; ++i) {
                if (!strncmp(message.topic, "energy/loads/", 13) &&
                    uint32_t(millis() - message.queuedAt) > 10000) continue;
                if (!strncmp(message.topic, "energy/loads/", 13) &&
                    !gridLoadsCommandCurrent(message.topic, message.payload)) continue;
                if (heatingPublisher_isTopic(message.topic) && nightRest) continue;
                if (client.publish(message.topic, message.payload, message.retained))
                    mqttActivity_record(false, message.topic, message.payload);
            }
        }
#endif
    }
    return client.connected();
}

static bool publish(const char *topic, const char *payload, bool retained = false)
{
    if (!messages || !topic || !payload) return false;
    MqttMessage message{};
    strlcpy(message.topic, topic, sizeof(message.topic));
    strlcpy(message.payload, payload, sizeof(message.payload));
    message.retained = retained;
    message.queuedAt = millis();
    return xQueueSend(messages, &message, 0) == pdTRUE;
}
bool mqtt_publish_retained(const char *topic, const char *payload) { return publish(topic, payload, true); }
bool mqtt_publish(const char *topic, const char *payload) { return publish(topic, payload); }
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
