#pragma once

#ifdef LOXONE_API
#include "energySource.h"

#ifndef LOXONE
#error "LOXONE_API requires LOXONE (Miniserver host)"
#endif

class LoxoneAsProvider : public EnergySource
{
public:
    bool init(Setup &setupData, WEBSOCK_DATA &webSockData) override;
    bool read(WEBSOCK_DATA &webSockData) override;

    const char* name() const override { return "loxone"; }
    EnergySourceType type() const override { return EnergySourceType::SOURCE_LOXONE; }
    bool hasBattery() const override { return m_hasBattery; }

private:
    bool m_hasBattery = false;
    bool readValue(const char* control, double &value);
};
#endif
