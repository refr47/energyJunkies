#pragma once

#include "energySource.h"
#include <atomic>
#include <freertos/semphr.h>

// Loxone supplies Fronius-compatible measurements through MQTT. The MQTT task
// owns its client; the Energy task only reads a protected copy of the values.
class LoxoneAsProvider : public EnergySource
{
public:
    bool init(Setup &setupData, WEBSOCK_DATA &webSockData) override;
    bool read(WEBSOCK_DATA &webSockData) override;
    bool hasPendingSample() const { return receivedRevision_.load() != publishedRevision_.load(); }

    const char *name() const override { return "loxone"; }
    EnergySourceType type() const override { return EnergySourceType::SOURCE_LOXONE; }
    bool hasBattery() const override { return true; }

private:
    static void mqttMessage(void *context, const char *topic, const uint8_t *payload, unsigned length);
    static void mqttConnection(void *context, bool connected, bool brokerChanged);
    void receive(const char *topic, const uint8_t *payload, unsigned length);

    SemaphoreHandle_t valuesMutex_ = nullptr;
    ENERGY_STATE values_{};
    uint8_t receivedMask_ = 0;
    std::atomic<uint32_t> receivedRevision_{0};
    std::atomic<uint32_t> publishedRevision_{0};
    std::atomic<bool> connected_{false};
};
