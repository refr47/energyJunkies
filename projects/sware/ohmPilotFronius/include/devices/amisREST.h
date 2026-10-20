#pragma once

#include "energySource.h"

// =============================================================================
// AmisREST — Adapter um die existierende AmisReader REST API
//
// Nutzt amisReader_initRestTargets() + amisReader_readRestTarget() als Backend.
 // =============================================================================

class AmisREST : public EnergySource
{
public:
    AmisREST() = default;
    ~AmisREST() override = default;

    bool init(Setup &setupData, WEBSOCK_DATA &webSockData) override;
    bool read(WEBSOCK_DATA &webSockData) override;

    const char*      name()   const override { return "AmisReader"; }
    EnergySourceType type()   const override { return EnergySourceType::SOURCE_AMIS; }
    bool          hasBattery() const override { return false; }
};

// Forward-Deklaration (existierende Funktionen in amisReader.cpp)
bool amisReader_initRestTargets(WEBSOCK_DATA &setup);
bool amisReader_readRestTarget(WEBSOCK_DATA &);
