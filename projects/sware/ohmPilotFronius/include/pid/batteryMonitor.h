#pragma once
#include <Arduino.h>
#include "debugConsole.h" // LOG_* macros + TAG_*

// =============================================================================
// BatteryMonitor — Battery & Grid Surplus Monitoring
//
// Encapsulates:
//   - Battery discharge guard (prevent heating when battery is discharging)
//   - Grid surplus calculation with battery awareness
//   - EMA smoothing of surplus values
//   - SOC threshold checks (battery priority mode)
//   - Test bias support (override available power for testing)
//
// Universal compatibility:
//   - AmisReader (no battery): battPower=0, soc=0 → guards disabled
//   - Fronius (with battery): full battery monitoring active
// =============================================================================
class BatteryMonitor
{
    friend class PinManager;

public:
    BatteryMonitor();

    // ── Update data from shared state (called under pidLockRead) ───────────
    void update(float gridPower, float battPower, float soc, 
                int akkuPriori, int wattSetupForTest);

    // ── Core: Calculate available power for heating ───────────────────────
    // Returns available power in watts, considering battery state and
    // grid surplus. Uses heaterPower() to get current heater draw for
    // self-compensation.
    int getAvailablePower(int heaterPower, bool &outWattBiasForTest);

    // ── State queries ────────────────────────────────────────────────────
    bool isBatteryCharging() const { return m_battPower < 0; }
    bool isBatteryOK() const;

private:
    // ── Configuration ────────────────────────────────────────────────────
    float m_akkuPriori = 0;      // Battery priority flag (1=active, 0=disabled)

    // ── Live data (updated via update()) ─────────────────────────────────
    float m_gridPower = 0;       // Grid power (W), negative = feeding
    float m_battPower = 0;       // Battery power (W), negative = discharging
    float m_soc = 0;             // State of charge (%)
    int   m_wattSetupForTest = 0; // Test override power

    // ── EMA state ────────────────────────────────────────────────────────
    float m_emaSurplus = 0;
    bool  m_emaInitialized = false;

    static constexpr float EMA_ALPHA = 0.15f;
    static constexpr int   BATT_THRESHOLD = 40; // SOC threshold (%)
};
