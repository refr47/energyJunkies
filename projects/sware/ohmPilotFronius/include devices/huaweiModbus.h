#pragma once

#include "energySource.h"

// =============================================================================
// HuaweiModbus — EnergySource via Modbus/TCP (Huawei SUN2000-10KTL-M1)
// Akku: Luna2000-2.0-kWh-S1
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

    const char*      name()   const override { return "huawei"; }
    EnergySourceType type()   const override { return EnergySourceType::SOURCE_HUAWEI; }
    bool          hasBattery() const override { return true; }

private:
    // ── Modbus/TCP-Verbindung ─────────────────────────────────────────
    bool      m_connected = false;
    IPAddress m_ip;
    WiFiClient m_client;
    uint16_t  m_transId = 0x0001;

    // ── Lokale Cache-Variablen (rohe Huawei-Werte) ────────────────────
    float m_p_pv_raw    = 0.0f;   // PV-Leistung (W)
    float m_p_grid_raw  = 0.0f;   // Netzleistung (W), Huawei-Konvention
    float m_p_batt_raw  = 0.0f;   // Batterieleistung (W), Huawei-Konvention
    float m_p_ac_raw    = 0.0f;   // AC-Out (W)
    uint16_t m_soc_raw  = 0;      // Batterie SoC (%)
    
    // Umwandlung auf Benutzer-Konvention
    // P_GRID: >0 Einspeisung, <0 Bezug  (Huawei: >0 Bezug, <0 Einspeisung) → negate
    // P_AKKU: >0 Bezug von Akku, <0 Laden (Huawei: >0 Laden, <0 Entlad)  → negate
    float m_p_pv   = 0.0f;
    float m_p_grid = 0.0f;
    float m_p_akku = 0.0f;
    float m_p_load = 0.0f;
    double m_soc   = 0.0;

    // ── Register-Helper ────────────────────────────────────────────────
    bool connect();
    bool disconnect();
    
    // atomares Batch-Read: liest N Registern in einem Modbus-Request
    bool readBatch(uint16_t startAddr, uint16_t quantity, uint16_t *buf);
    
    // Float32 aus 2 Registern (ABCD-Byte order, Big-Endian)
    static float parseFloat32(uint16_t hi, uint16_t lo);

    // ── Huawei SUN2000-10KTL-M1 Register-Map ──────────────────────────
    // Quelle: Huawei-SUN2000-Modbus-Communication-Protocol-01V01-EN
    // Real-time Monitoring Data (Read Holding Register FC 03H)
    //
    // Batch 1: PV-Leistung         0x7D80 (2 regs, Float32)
    // Batch 2: AC-Out + Gier       0x7866 (2 regs, Float32)
    // Batch 3: Netz-Bat            0x9129 (0x9129 (2 regs, Float32)
    // Batch 4: SoC + Battery Power  0x9390 (0x9390 (4 regs, Uints + Float32)

private:
    // ── Modbus TCP raw read helpers ────────────────────────────────────
    // Liest ein 32-Bit Float-Speicher ab 'registerAddr'.
    // Blockiert mit vTaskDelay(500ms).

    // ── Private helper: Modbus TCP raw read 16-Bit ───────────────────
};
