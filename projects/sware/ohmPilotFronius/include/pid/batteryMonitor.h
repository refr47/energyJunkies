#pragma once
#include <Arduino.h>
#include "debugConsole.h" // LOG_* macros + TAG_*


#define BAT_PRIORITY_OFF 0
#define BAT_PRIORITY_ON  1
#define BAT_PRIORITY_ALLOW_DISCHARGE 2
#define BAT_POWER_TRESHOLD 30  // Discharge tolerance in watts
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
                int akkuPriori, int wattSetupForTest,
                uint32_t sampleSequence, int heaterPowerAtSample);

    // ── Core: Calculate available power for heating ───────────────────────
    // Returns available power in watts, considering battery state and
    // grid surplus. Uses only the heater reference captured with the sample.
    // Repeated calls hold the budget until a new sample or setup change.
    int getAvailablePower(bool &outWattBiasForTest);

    // ── State queries ────────────────────────────────────────────────────
    bool isBatteryCharging() const { return m_battPower < 0; }
    bool isBatteryOK() const;

private:
    // ── Configuration ────────────────────────────────────────────────────
    int m_akkuPriori = 0;        // 0=release charging power, 1=charge first, 2=allow discharge

    // ── Live data (updated via update()) ─────────────────────────────────
    float m_gridPower = 0;       // Grid power (W), negative = feeding
    float m_battPower = 0;       // Battery power (W), negative = charging, positive = discharging
    float m_soc = 0;             // State of charge (%)
    int   m_wattSetupForTest = 0; // Test override power

    uint32_t m_sampleSequence = 0;
    int m_heaterPowerAtSample = 0;
    bool m_budgetDirty = false;
    int m_availablePower = 0;

    // ── EMA state ────────────────────────────────────────────────────────
    float m_emaSurplus = 0;
    bool  m_emaInitialized = false;

    static constexpr float EMA_ALPHA = 0.15f;
    static constexpr int   BATT_THRESHOLD = 40; // SOC threshold (%)
};
