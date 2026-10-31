#pragma once

#include <atomic>
#include <Arduino.h>
#include "defines.h"

// =============================================================================
// EnergySource — Abstrakte Schnittstelle für Inverter/Smart-Meter
//
// Jede physikalische Datenquelle (Fronius HTTP, AmisReader HTTP, Huawei Modbus)
 // implementiert diese Klasse. Nur eine Instanz ist aktiv →
 // 0 #ifdef im Hot-Path (PID, Influx, WebSockets, TFT).
 // =============================================================================

enum class EnergySourceType {
    SOURCE_NONE = 0,
    SOURCE_FRONIUS,
    SOURCE_AMIS,
    SOURCE_HUAWEI,
    SOURCE_LOXONE
};

class EnergySource
{
public:
    virtual ~EnergySource() = default;

    // ── Setup (einmalig bei Boot / nach Reconnect) ────────────────────────
    // Gibt true zurück, wenn die Quelle erreichbar ist.
    // Dürfte blockierende Netzwerkanfragen stellen.
    virtual bool init(Setup &setupData, WEBSOCK_DATA &webSockData) = 0;

    // ── Datenaktualisierung (alle ~40s) ──────────────────────────────────
    // Schreibt in webSockData.energyState  (vereinheitlicht)
    // Gibt true zurück bei erfolgreichem Read.
    virtual bool read(WEBSOCK_DATA &webSockData) = 0;

    // ── Quelle-Identifikation ────────────────────────────────────────────
    virtual const char*   name()  const = 0;
    virtual EnergySourceType type() const = 0;
    virtual bool          hasBattery() const { return false; }

protected:
    // Publish a complete measurement and its heater reference atomically.
    // Network I/O must finish before this short critical section.
    static bool publishSample(WEBSOCK_DATA &data, ENERGY_STATE sample);
};

// ── Globale Pointer — auf exakt eine aktive Instanz zeigen ──────────────
// In app_services.cpp gesetzt, von niemandem mehr mit #ifdef gelesen.

inline std::atomic<EnergySource*> g_activeSource{nullptr};
inline EnergySourceType g_sourceType = EnergySourceType::SOURCE_NONE;
