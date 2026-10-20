#pragma once

#include "energySource.h"

// =============================================================================
// HuaweiModbus — EnergySource via Modbus/TCP (Huawei SUN2000)
//
// Liest Register-Blöcke: PV-Leistung, Netz, Last, Batterie SoC + Leistung.
 // =============================================================================

class HuaweiModbus : public EnergySource
{
public:
    HuaweiModbus() = default;
    ~HuaweiModbus() override = default;

    bool init(Setup &setupData, WEBSOCK_DATA &webSockData) override;
    bool read(WEBSOCK_DATA &webSockData) override;

    const char*      name()   const override { return "Huawei SUN2000"; }
    EnergySourceType type()   const override { return EnergySourceType::SOURCE_HUAWEI; }
    bool          hasBattery() const override { return true; }

private:
    // ── Modbus/TCP-Verbindung ─────────────────────────────────────────
    bool m_connected = false;
    IPAddress m_ip;
    WiFiClient m_client;

    // ── Register-Helper ────────────────────────────────────────────────
    bool connect();
    bool disconnect();
    float readFloat(uint16_t registerAddr);
    float readFloatRaw(int16_t raw, float scale);

    // ── Huawei SUN2000 Register-Map (Beispielwerte — anpassen!) ─────
    // DC Input (Inverter):
    static const uint16_t HUAWAI_REG_AC_POWER     = 0x153A; // 5434, Active Power (W)
    static const uint16_t HUAWAI_REG_GRID_POWER   = 0x0048; // 72,  Grid Power (W)
    static const uint16_t HUAWAI_REG_LOAD_POWER   = 0x0049; // 73,  Load Power (W) -- approx.
    static const uint16_t HUAWAI_REG_BATT_POWER   = 0x3365; // 13157, Battery P (W)
    static const uint16_t HUAWAI_REG_BATT_SOC     = 0x30D2; // 12498, Battery SoC (0.1%)
    static const uint16_t HUAWAI_REG_PV_POWER     = 0x1537; // 5431, PV total power
    static const uint16_t HUAWAI_REG_DEVICE_ID    = 1;      // Slave ID

private:
    // ── Private helper: Modbus TCP raw read ────────────────────────────
    // Liest ein 32-Bit Float-Speicher ab 'registerAddr'.
    // Blockiert mit vTaskDelay(500ms).
    bool readFloat32(uint16_t registerAddr, float &result);

    // ── Private helper: Modbus TCP raw read 16-Bit ───────────────────
    bool readU16(uint16_t registerAddr, uint16_t &data);
};
