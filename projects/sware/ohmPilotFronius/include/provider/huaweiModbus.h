#pragma once

#include "energySource.h"

// Huawei SUN2000 via Modbus/TCP, FC03, Unit-ID 1.
// Leistungen: I32, Big-Endian (High-Word zuerst), Rohwert in W.
// ENERGY_STATE: Netzbezug positiv, Hausverbrauch negativ,
// Batterieentladung positiv. Huawei liefert Netzeinspeisung und Ladung positiv.

class HuaweiModbus : public EnergySource
{
public:
    HuaweiModbus() = default;
    ~HuaweiModbus() override = default;

    bool init(Setup &setupData, WEBSOCK_DATA &webSockData) override;
    bool read(WEBSOCK_DATA &webSockData) override;

    const char*      name()   const override { return "huawei"; }
    EnergySourceType type()   const override { return EnergySourceType::SOURCE_HUAWEI; }
    bool           hasBattery() const override { return true; }

private:
    // ── Modbus/TCP-Verbindung ─────────────────────────────────────────
    bool       m_connected = false;
    IPAddress  m_ip;

    // Registeradressen sind dezimal, ohne 40001-Offset.
    static const uint16_t HUAWAI_REG_PV_POWER     = 32064; // I32, W, 0x7D40
    static const uint16_t HUAWAI_REG_ACTIVE_POWER = 32080; // I32, W, 0x7D50
    static const uint16_t HUAWAI_REG_GRID_POWER   = 37113; // I32, W, 0x90F9
    static const uint16_t HUAWAI_REG_BATT_SOC     = 37760; // U16, 0.1%, 0x9380
    static const uint16_t HUAWAI_REG_BATT_POWER   = 37765; // I32, W, 0x9385
    static const uint8_t  HUAWAI_REG_DEVICE_ID    = 1;

    // ── Register-Helper ────────────────────────────────────────────────
    bool connect(Setup &setupData);
    bool readBatch(uint16_t startAddr, uint16_t quantity, uint16_t *buf);
    uint8_t readBatchOnce(uint16_t startAddr, uint16_t quantity, uint16_t *buf);
    static int32_t parseInt32(uint16_t hi, uint16_t lo);
};