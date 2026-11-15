#ifdef LOXONE_API

#include "loxoneAsProvider.h"

#include <mqtt_client.h>
#include <esp_mac.h>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <freertos/task.h>

#include "app_state.h"
#include "debugConsole.h"
#ifdef FEATURE_MQTT
#include "mqttActivity.h"
#endif

namespace {
    // One table is used both for MQTT subscriptions and for field assignment.
    // Build flags may override these defaults to match a Loxone installation.
#ifndef LOXONE_TOPIC_P_GRID
#define LOXONE_TOPIC_P_GRID "fronius/p_grid_watt"
#endif
#ifndef LOXONE_TOPIC_P_LOAD
#define LOXONE_TOPIC_P_LOAD "fronius/p_load_watt"
#endif
#ifndef LOXONE_TOPIC_P_PV
#define LOXONE_TOPIC_P_PV "fronius/p_pv_watt"
#endif
#ifndef LOXONE_TOPIC_P_AKKU
#define LOXONE_TOPIC_P_AKKU "fronius/p_batt_watt"
#endif
#ifndef LOXONE_TOPIC_SOC
#define LOXONE_TOPIC_SOC "fronius/soc_percent"
#endif


    struct TopicField {
        const char* topic;
        double ENERGY_STATE::* field;
    };

    constexpr TopicField topics[] = {
        { LOXONE_TOPIC_P_GRID, &ENERGY_STATE::p_grid },
        { LOXONE_TOPIC_P_LOAD, &ENERGY_STATE::p_load },
        { LOXONE_TOPIC_P_PV, &ENERGY_STATE::p_pv },
        { LOXONE_TOPIC_P_AKKU, &ENERGY_STATE::p_akku },
        { LOXONE_TOPIC_SOC, &ENERGY_STATE::soc },
    };
    constexpr uint8_t allTopics = (1U << (sizeof(topics) / sizeof(topics[0]))) - 1U;
    constexpr uint32_t retryMs = 5000;

    bool parseNumber(const uint8_t* payload, unsigned length, double& number)
    {
        if (!payload || !length || length >= 48) return false;
        char text[48];
        memcpy(text, payload, length);
        text[length] = '\0';
        char* end = nullptr;
        errno = 0;
        const double parsed = std::strtod(text, &end);
        if (end == text || errno == ERANGE || !std::isfinite(parsed)) return false;
        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
        if (*end != '\0') return false;
        number = parsed;
        return true;
    }
} // namespace

bool LoxoneAsProvider::init(Setup& setupData, WEBSOCK_DATA& webSockData)
{
    (void)webSockData;
    LOG_INFO(TAG_LOXONE, "Loxone: initializing MQTT connection");
    if (!setupData.mqttEnabled || !setupData.mqttSetup.mqttHost[0] || !setupData.mqttPort) {
        LOG_ERROR(TAG_LOXONE, "Loxone: MQTT broker is not configured/enabled");
        return false;
    }
    initialConfig_ = setupData.mqttSetup;
    initialPort_ = setupData.mqttPort;
    initialEnabled_ = setupData.mqttEnabled;
    valuesMutex_ = xSemaphoreCreateMutex();
    if (!valuesMutex_) return false;
    if (xTaskCreate(mqttTaskEntry, "Loxone MQTT", 6144, this, 1, nullptr) != pdPASS) {
        vSemaphoreDelete(valuesMutex_);
        valuesMutex_ = nullptr;
        LOG_ERROR(TAG_LOXONE, "Loxone: could not start MQTT task");
        return false;
    }
    LOG_INFO(TAG_LOXONE, "Loxone: initializing MQTT connection and return true");
    return true;
}

bool LoxoneAsProvider::read(WEBSOCK_DATA& webSockData)
{
    LOG_INFO(TAG_LOXONE, "Loxone: read BEGIN");
    if (!connected_.load() || !valuesMutex_) return false;
    ENERGY_STATE sample{};
    if (xSemaphoreTake(valuesMutex_, pdMS_TO_TICKS(50)) != pdTRUE) {
        LOG_ERROR(TAG_LOXONE, "Loxone: read - no mutext available");
        return false;

    }
    const bool complete = receivedMask_ == allTopics;
    if (complete) sample = values_;
    xSemaphoreGive(valuesMutex_);
    if (!complete || !connected_.load()) {
        LOG_ERROR(TAG_LOXONE, "Loxone: read - no complete data available (mask=%02X, connected=%d)", receivedMask_, connected_.load());
        return false;
    }

    bool success = publishSample(webSockData, sample);
    LOG_INFO(TAG_LOXONE, "Loxone: read END success=%d", success);
    return success;
}

void LoxoneAsProvider::markDisconnected(bool discardValues)
{
    connected_.store(false);
    // A temporary disconnect does not erase values: Loxone only sends changes
    // and does not retain messages. A broker change starts a new data set.
    if (discardValues && xSemaphoreTake(valuesMutex_, portMAX_DELAY) == pdTRUE) {
        receivedMask_ = 0;
        xSemaphoreGive(valuesMutex_);
    }
}

void LoxoneAsProvider::receive(const char* topic, const uint8_t* payload, unsigned length)
{
    double value;
    if (!parseNumber(payload, length, value)) {
        LOG_WARNING(TAG_LOXONE, "Loxone: invalid MQTT value on %s", topic);
        return;
    }
    for (unsigned i = 0; i < sizeof(topics) / sizeof(topics[0]); ++i) {
        if (strcmp(topic, topics[i].topic) != 0) continue;
        if (topics[i].field == &ENERGY_STATE::soc && (value < 0 || value > 100)) {
            LOG_WARNING(TAG_LOXONE, "Loxone: SOC out of range");
            return;
        }
        if (xSemaphoreTake(valuesMutex_, portMAX_DELAY) == pdTRUE) {
            values_.*(topics[i].field) = value;
            receivedMask_ |= 1U << i;
            xSemaphoreGive(valuesMutex_);
#ifdef FEATURE_MQTT
            char valueText[33];
            snprintf(valueText, sizeof(valueText), "%.3f", value);
            mqttActivity_record(true, topic, valueText);
#endif
        }
        return;
    }
}

void LoxoneAsProvider::mqttTaskEntry(void* context)
{
    static_cast<LoxoneAsProvider*>(context)->mqttTask();
}

void LoxoneAsProvider::mqttEvent(void* context, const char*, int32_t eventId, void* eventData)
{
    auto* self = static_cast<LoxoneAsProvider*>(context);
    auto* event = static_cast<esp_mqtt_event_handle_t>(eventData);
    switch (eventId) {
    case MQTT_EVENT_CONNECTED: {
        bool subscribed = true;
        for (const auto& entry : topics) {
            if (esp_mqtt_client_subscribe(event->client, entry.topic, 0) < 0) subscribed = false;
        }
        if (subscribed) {
            self->connected_.store(true);
            LOG_INFO(TAG_LOXONE, "Loxone: subscribed to energy topics");
        }
        else {
            self->markDisconnected();
            self->subscriptionFailed_.store(true);
            LOG_ERROR(TAG_LOXONE, "Loxone: MQTT subscription failed");
        }
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        self->markDisconnected();
        break;
    case MQTT_EVENT_DATA: {
        // ESP-IDF does not NUL-terminate topics and may split large payloads.
        if (event->current_data_offset != 0 || event->data_len != event->total_data_len ||
            event->data_len <= 0 || event->data_len >= 48) break;
        for (const auto& entry : topics) {
            const size_t topicLength = strlen(entry.topic);
            if (event->topic_len == static_cast<int>(topicLength) &&
                memcmp(event->topic, entry.topic, topicLength) == 0) {
                self->receive(entry.topic, reinterpret_cast<const uint8_t*>(event->data),
                    static_cast<unsigned>(event->data_len));
                break;
            }
        }
        break;
    }
    default:
        break;
    }
}

void LoxoneAsProvider::mqttTask()
{
    char clientId[40];
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(clientId, sizeof(clientId), "Energy-Harvester-Loxone-%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    MQTT_SETUP config = initialConfig_;
    unsigned short port = initialPort_;
    bool enabled = initialEnabled_;
    esp_mqtt_client_handle_t client = nullptr;
    uint32_t lastAttempt = 0;
    bool attempted = false;

    for (;;) {
        MQTT_SETUP next = config;
        unsigned short nextPort = port;
        bool nextEnabled = enabled;
        if (appLockConfig(50)) {
            next = g_app.webSockData.setupData.mqttSetup;
            nextPort = g_app.webSockData.setupData.mqttPort;
            nextEnabled = g_app.webSockData.setupData.mqttEnabled;
            appUnlockConfig();
        }
        if (subscriptionFailed_.exchange(false) && client) {
            esp_mqtt_client_stop(client);
            esp_mqtt_client_destroy(client);
            client = nullptr;
            attempted = true;
            lastAttempt = millis();
        }
        if (nextEnabled != enabled || nextPort != port || memcmp(&next, &config, sizeof(config))) {
            if (client) {
                esp_mqtt_client_stop(client);
                esp_mqtt_client_destroy(client);
                client = nullptr;
            }
            markDisconnected(true);
            config = next;
            port = nextPort;
            enabled = nextEnabled;
            attempted = false;
        }

        const uint32_t now = millis();
        if (enabled && config.mqttHost[0] && port && !client &&
            (!attempted || uint32_t(now - lastAttempt) >= retryMs)) {
            attempted = true;
            lastAttempt = now;
            esp_mqtt_client_config_t mqttConfig{};
            mqttConfig.broker.address.hostname = config.mqttHost;
            mqttConfig.broker.address.port = port;
            mqttConfig.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
            mqttConfig.credentials.client_id = clientId;
            mqttConfig.credentials.username = config.mqttUser[0] ? config.mqttUser : nullptr;
            mqttConfig.credentials.authentication.password = config.mqttPass;
            mqttConfig.session.keepalive = 30;
            mqttConfig.network.reconnect_timeout_ms = retryMs;
            mqttConfig.network.timeout_ms = 2000;
            client = esp_mqtt_client_init(&mqttConfig);
            if (client && (esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqttEvent, this) != ESP_OK ||
                esp_mqtt_client_start(client) != ESP_OK)) {
                esp_mqtt_client_destroy(client);
                client = nullptr;
                LOG_ERROR(TAG_LOXONE, "Loxone: could not start MQTT client");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

#endif // LOXONE_API
