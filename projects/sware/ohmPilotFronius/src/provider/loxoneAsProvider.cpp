#ifdef LOXONE_API

#include "loxoneAsProvider.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "debugConsole.h"
#ifdef FEATURE_MQTT
#include "mqtt.h"
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
#define LOXONE_TOPIC_SOC "fronius/p_socc_prozent"
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
#ifdef FEATURE_MQTT
    if (!setupData.mqttEnabled || !setupData.mqttSetup.mqttHost[0] || !setupData.mqttPort) {
        LOG_ERROR(TAG_LOXONE, "Loxone: MQTT broker is not configured/enabled");
        return false;
    }
    valuesMutex_ = xSemaphoreCreateMutex();
    if (!valuesMutex_) return false;
    static const char *const topicNames[] = {
        LOXONE_TOPIC_P_GRID, LOXONE_TOPIC_P_LOAD, LOXONE_TOPIC_P_PV,
        LOXONE_TOPIC_P_AKKU, LOXONE_TOPIC_SOC
    };
    if (!mqtt_register_receiver(topicNames, sizeof(topicNames) / sizeof(topicNames[0]),
                                this, mqttMessage, mqttConnection)) {
        vSemaphoreDelete(valuesMutex_);
        valuesMutex_ = nullptr;
        LOG_ERROR(TAG_LOXONE, "Loxone: could not register MQTT receiver");
        return false;
    }
    return true;
#else
    (void)setupData;
    LOG_ERROR(TAG_LOXONE, "Loxone: FEATURE_MQTT is required");
    return false;
#endif
}

bool LoxoneAsProvider::read(WEBSOCK_DATA& webSockData)
{
    LOG_INFO(TAG_LOXONE, "Loxone: read BEGIN");
    if (!connected_.load() || !valuesMutex_) return false;
    ENERGY_STATE sample{};
    uint8_t receivedMask = 0;
    if (xSemaphoreTake(valuesMutex_, pdMS_TO_TICKS(50)) != pdTRUE) {
        LOG_ERROR(TAG_LOXONE, "Loxone: read - no mutex available");
        return false;

    }
    // Topics arrive independently. Unseen fields stay at zero; received
    // fields keep their last value until that topic is updated again.
    receivedMask = receivedMask_;
    if (receivedMask) sample = values_;
    xSemaphoreGive(valuesMutex_);
    if (!receivedMask || !connected_.load()) {
        LOG_ERROR(TAG_LOXONE, "Loxone: read - no data available (mask=%02X, connected=%d)", receivedMask, connected_.load());
        return false;
    }

    bool success = publishSample(webSockData, sample);
    LOG_INFO(TAG_LOXONE,
             "Loxone: read END success=%d mask=%02X grid=%.3f load=%.3f pv=%.3f batt=%.3f soc=%.3f",
             success, receivedMask, sample.p_grid, sample.p_load, sample.p_pv,
             sample.p_akku, sample.soc);
    return success;
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
            const uint8_t mask = receivedMask_;
            xSemaphoreGive(valuesMutex_);
            LOG_INFO(TAG_LOXONE, "Loxone: received %s = %.3f (mask=%02X)",
                     topic, value, mask);
#ifdef FEATURE_MQTT
            char valueText[33];
            snprintf(valueText, sizeof(valueText), "%.3f", value);
            mqttActivity_record(true, topic, valueText);
#endif
        }
        return;
    }
    LOG_WARNING(TAG_LOXONE, "Loxone: unexpected MQTT topic %s", topic);
}
 
void LoxoneAsProvider::mqttMessage(void* context, const char* topic,
                                   const uint8_t* payload, unsigned length)
{
    static_cast<LoxoneAsProvider*>(context)->receive(topic, payload, length);
}

void LoxoneAsProvider::mqttConnection(void* context, bool connected, bool brokerChanged)
{
    auto* self = static_cast<LoxoneAsProvider*>(context);
    self->connected_.store(connected);
    if (brokerChanged && self->valuesMutex_ &&
        xSemaphoreTake(self->valuesMutex_, portMAX_DELAY) == pdTRUE) {
        self->values_ = {};
        self->receivedMask_ = 0;
        xSemaphoreGive(self->valuesMutex_);
    }
    if (connected)
        LOG_INFO(TAG_LOXONE, "Loxone: subscribed to energy topics");
}

#endif // LOXONE_API
