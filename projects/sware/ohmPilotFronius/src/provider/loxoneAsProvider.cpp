#ifdef LOXONE_API

#include "loxoneAsProvider.h"

#include <PubSubClient.h>
#include <WiFi.h>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <freertos/task.h>

#include "app_state.h"
#include "debugConsole.h"

namespace {
// One table is used both for MQTT subscriptions and for field assignment.
// Build flags may override these defaults to match a Loxone installation.
#ifndef LOXONE_TOPIC_P_GRID
#define LOXONE_TOPIC_P_GRID "site/P_Grid"
#endif
#ifndef LOXONE_TOPIC_P_LOAD
#define LOXONE_TOPIC_P_LOAD "site/P_Load"
#endif
#ifndef LOXONE_TOPIC_P_PV
#define LOXONE_TOPIC_P_PV "site/P_PV"
#endif
#ifndef LOXONE_TOPIC_P_AKKU
#define LOXONE_TOPIC_P_AKKU "site/P_Akku"
#endif
#ifndef LOXONE_TOPIC_SOC
#define LOXONE_TOPIC_SOC "inverters/1/SOC"
#endif

struct TopicField {
    const char *topic;
    double ENERGY_STATE::*field;
};

constexpr TopicField topics[] = {
    {LOXONE_TOPIC_P_GRID, &ENERGY_STATE::p_grid},
    {LOXONE_TOPIC_P_LOAD, &ENERGY_STATE::p_load},
    {LOXONE_TOPIC_P_PV, &ENERGY_STATE::p_pv},
    {LOXONE_TOPIC_P_AKKU, &ENERGY_STATE::p_akku},
    {LOXONE_TOPIC_SOC, &ENERGY_STATE::soc},
};
constexpr uint8_t allTopics = (1U << (sizeof(topics) / sizeof(topics[0]))) - 1U;
constexpr uint32_t retryMs = 5000;

bool parseNumber(const uint8_t *payload, unsigned length, double &number)
{
    if (!payload || !length || length >= 48) return false;
    char text[48];
    memcpy(text, payload, length);
    text[length] = '\0';
    char *end = nullptr;
    errno = 0;
    const double parsed = std::strtod(text, &end);
    if (end == text || errno == ERANGE || !std::isfinite(parsed)) return false;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
    if (*end != '\0') return false;
    number = parsed;
    return true;
}
} // namespace

bool LoxoneAsProvider::init(Setup &setupData, WEBSOCK_DATA &webSockData)
{
    (void)webSockData;
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
    return true;
}

bool LoxoneAsProvider::read(WEBSOCK_DATA &webSockData)
{
    if (!connected_.load() || !valuesMutex_) return false;
    ENERGY_STATE sample{};
    if (xSemaphoreTake(valuesMutex_, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    const bool complete = receivedMask_ == allTopics;
    if (complete) sample = values_;
    xSemaphoreGive(valuesMutex_);
    if (!complete || !connected_.load()) return false;
    return publishSample(webSockData, sample);
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

void LoxoneAsProvider::receive(const char *topic, const uint8_t *payload, unsigned length)
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
        }
        return;
    }
}

void LoxoneAsProvider::mqttTaskEntry(void *context)
{
    static_cast<LoxoneAsProvider *>(context)->mqttTask();
}

void LoxoneAsProvider::mqttTask()
{
    WiFiClient transport;
    PubSubClient client(transport);
    client.setSocketTimeout(2);
    client.setKeepAlive(30);
    client.setCallback([this](char *topic, uint8_t *payload, unsigned length) {
        receive(topic, payload, length);
    });
    char clientId[40];
    snprintf(clientId, sizeof(clientId), "Energy-Harvester-Loxone-%012llX",
             static_cast<unsigned long long>(ESP.getEfuseMac()));

    MQTT_SETUP config = initialConfig_;
    unsigned short port = initialPort_;
    bool enabled = initialEnabled_;
    client.setServer(config.mqttHost, port);
    uint32_t lastAttempt = 0;
    bool attempted = false;
    bool subscribed = false;

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
        if (nextEnabled != enabled || nextPort != port || memcmp(&next, &config, sizeof(config))) {
            client.disconnect();
            markDisconnected(true);
            config = next;
            port = nextPort;
            enabled = nextEnabled;
            subscribed = attempted = false;
            if (enabled && config.mqttHost[0] && port) client.setServer(config.mqttHost, port);
        }

        if (!enabled || !config.mqttHost[0] || !port || WiFi.status() != WL_CONNECTED) {
            if (client.connected()) client.disconnect();
            if (subscribed) markDisconnected();
            subscribed = attempted = false;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        const uint32_t now = millis();
        if (!client.connected()) {
            if (subscribed) markDisconnected();
            subscribed = false;
            if (!attempted || uint32_t(now - lastAttempt) >= retryMs) {
                attempted = true;
                lastAttempt = now;
                if (client.connect(clientId,
                        config.mqttUser[0] ? config.mqttUser : nullptr, config.mqttPass)) {
                    bool ok = true;
                    for (const auto &entry : topics) ok = client.subscribe(entry.topic) && ok;
                    if (ok) {
                        subscribed = true;
                        connected_.store(true);
                        LOG_INFO(TAG_LOXONE, "Loxone: subscribed to energy topics");
                    } else {
                        client.disconnect();
                        markDisconnected();
                        LOG_ERROR(TAG_LOXONE, "Loxone: MQTT subscription failed");
                    }
                }
            }
        }
        if (subscribed && !client.loop()) {
            markDisconnected();
            subscribed = false;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

#endif // LOXONE_API
