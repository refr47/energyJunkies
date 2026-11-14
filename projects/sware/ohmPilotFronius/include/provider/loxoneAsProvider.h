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

    const char *name() const override { return "loxone"; }
    EnergySourceType type() const override { return EnergySourceType::SOURCE_LOXONE; }
    bool hasBattery() const override { return true; }

private:
    static void mqttTaskEntry(void *context);
    void mqttTask();
    void receive(const char *topic, const uint8_t *payload, unsigned length);
    void markDisconnected(bool discardValues = false);

    SemaphoreHandle_t valuesMutex_ = nullptr;
    ENERGY_STATE values_{};
    uint8_t receivedMask_ = 0;
    std::atomic<bool> connected_{false};
    MQTT_SETUP initialConfig_{};
    unsigned short initialPort_ = 0;
    bool initialEnabled_ = false;
};
