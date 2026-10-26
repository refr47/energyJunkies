#pragma once

#include "energySource.h"

// =============================================================================
// HuaweiModbus — EnergySource via Modbus/TCP (Huawei SUN2000-10KTL-M1)
// Akku: Luna2000-2.0-kWh-S1
//
// Quelle: Huawei-SUN2000-Modbus-Communication-Protocol-01V01-EN
// Real-time Monitoring Data (Read Holding Register FC 03H)
// Float32 = Big-Endian ABCD (2 Register je)
//
// Zeichenkonvention (Benutzer):
//   P_AKKU >0 : Bezug von Akku (Entladen), <0 : Akku laden
//   P_GRID >0 : Einspeisung ins Netz,           <0 : Bezug vom Netz
//   P_PV   : aktuelle PV-Ernte (W)
//   P_LOAD : Hausverbrauch (W)
//
// Huawei-Rohkonvention:
//   0x9129 Grid Power  : +1 = Bezug vom Netz, -1 = Einspeisung  → negate
//   0x9392 Batt Power  : +1 = Laden, -1 = Entladen                → negate
//
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
    bool           hasBattery() const override { return true; }

private:
    // ── Modbus/TCP-Verbindung ─────────────────────────────────────────
    bool       m_connected = false;
    IPAddress  m_ip;
    WiFiClient m_client;
    uint16_t   m_transId = 0x0001;

    // ── Lokale Cache-Variablen (bereits benutzer-konvention) ──────────
    float  m_p_pv   = 0.0f;
    float  m_p_grid = 0.0f;
    float  m_p_akku = 0.0f;
    float  m_p_load = 0.0f;
    double m_soc    = 0.0;

    // ── Modbus/TCP-Register-Konstanten ────────────────────────────────
    // Drei Batches:
    //   Batch 1: 0x7D80 (PV Power,         2 regs, Float32)
    //   Batch 2: 0x9129 (Grid Power,       2 regs, Float32)
    //   Batch 3: 0x9390 (SoC + Batt Power, 3 regs, U16+Float32)
    //
    static const uint16_t HUAWAI_REG_PV_POWER   = 32064;  //aktuelle pv eingangsleistung (W) 0x7D80
    static const uint16_t HUAWAI_REG_ACTIVE_POWER   = 32080;  //aktuelle ausgangsleistung wechselrichter ab-/einspeisung akku > 0: einspeisung < 0: bezug; 0x7D90
    // wenn akku geladen wird, dann niedriger als PV_Power, da mit gleichstrom akku geladen wird und vom wechselrichter erst gar nicht geladen wird;
    // bei akk entladen > PV_POWER, da gleichstrom vom akku gewandelt wird
    static const uint16_t HUAWAI_REG_GRID_POWER = 37113;  //aktuelle netz bezugsleistung (W) > 0: einspeisung < 0 bezug; 0x9129
    static const uint16_t HUAWAI_REG_BATT_SOC   = 37760; //aktueller akku ladestand (SoC) in % 0x9390
    static const uint16_t HUAWAI_REG_BATT_POWER = 37765; //aktuelle batterie lade/entladeleistung (W) < 0: entladen > 0 laden; 0x9392
    static const uint16_t HUAWAI_REG_DEVICE_ID  = 1;

    // ── Register-Helper ────────────────────────────────────────────────
    bool connect();
    bool disconnect();
    bool readBatch(uint16_t startAddr, uint16_t quantity, uint16_t *buf);
    static float parseFloat32(uint16_t hi, uint16_t lo);
};
