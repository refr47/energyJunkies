#pragma once

#include "energySource.h"

// =============================================================================
// FroniusHTTP — Adapter um die existierende Fronius Solar Power Flow API
//
// Nutzt soloar_init() + solar_get_powerflow() als Backend.
// Diese Funktionen bleiben unverändert — kein Risiko für Fronius-User.
 // =============================================================================

class FroniusHTTP : public EnergySource
{
public:
    FroniusHTTP() = default;
    ~FroniusHTTP() override = default;

    bool init(Setup &setupData, WEBSOCK_DATA &webSockData) override;
    bool read(WEBSOCK_DATA &webSockData) override;

    const char*      name()   const override { return "fronius"; }
    EnergySourceType type()   const override { return EnergySourceType::SOURCE_FRONIUS; }
    bool             hasBattery() const override;
};

// Forward-Deklaration (existierende Funktionen in froniusSolarAPI.cpp)
bool soloar_init(WEBSOCK_DATA &webSockData, bool *akku);
bool solar_get_powerflow(WEBSOCK_DATA &webSockData);
