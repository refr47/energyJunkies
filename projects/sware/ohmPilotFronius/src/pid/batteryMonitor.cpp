// =============================================================================
// batteryMonitor.cpp — Battery & Grid Surplus Monitoring Implementation
// =============================================================================

#include "batteryMonitor.h"
#include "debugConsole.h"

// ──────────────────────────────────────────────────────────────────────────
// Constructor
// ──────────────────────────────────────────────────────────────────────────
BatteryMonitor::BatteryMonitor()
    : m_akkuPriori(0),
      m_gridPower(0),
      m_battPower(0),
      m_soc(0),
      m_wattSetupForTest(0),
      m_emaSurplus(0),
      m_emaInitialized(false)
{
}

// ──────────────────────────────────────────────────────────────────────────
// update — refresh live data from shared state
// ──────────────────────────────────────────────────────────────────────────
void BatteryMonitor::update(float gridPower, float battPower, float soc,
                           int akkuPriori, int wattSetupForTest)
{
    m_gridPower = gridPower;
    m_battPower = battPower;
    m_soc = soc;
    m_akkuPriori = akkuPriori;
    m_wattSetupForTest = wattSetupForTest;
}

// ──────────────────────────────────────────────────────────────────────────
// isBatteryOK — battery state health check
// ──────────────────────────────────────────────────────────────────────────
bool BatteryMonitor::isBatteryOK() const
{
    // AmisReader (no battery): soc=0 → always OK
    // Fronius (with battery): check if battery is not discharging
    return (m_soc <= 0) || (m_battPower >= 0);
}

// ──────────────────────────────────────────────────────────────────────────
// getAvailablePower — calculate available power for heating
//
// Universal: Works with both AmisReader (battPower=0) and Fronius (battPower≠0)
// Steps:
//   1. Guard: Battery discharging → return 0
//   2. Calculate raw surplus (grid + battery aware)
//   3. Compensate for current heater draw (only when drawing from grid)
//   4. Apply EMA smoothing to reduce jitter
//   5. Check SOC threshold (battery priority mode)
//   6. Apply test bias override
// ──────────────────────────────────────────────────────────────────────────
int BatteryMonitor::getAvailablePower(int heaterPower, bool &outWattBiasForTest)
{
    // ━━━ Step 1: Battery discharge guard ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━───
    // Battery discharging → don't heat with battery energy
    if (m_battPower < 0) {
        LOG_INFO(TAG_BATT, "BLOCKED batt discharging %.0fW", m_battPower);
        return 0; 
    }

    // AmisReader (m_battPower == 0) → always proceeds past this guard

    // ━━━ Step 2: Raw surplus calculation ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━───
    // p_grid < 0 → feeding (surplus), p_grid > 0 → drawing from grid
    // -m_battPower: when charging (>0) reduces surplus, when neutral (0) no effect
    float measuredSurplus = -m_gridPower - m_battPower;

    // Self-compensation: only when actually drawing from grid
    // Otherwise we'd add previous state to an already-negative balance
    if (m_gridPower > 0)
    {
        measuredSurplus += heaterPower;
    }

    // ━━━ Step 3: EMA smoothing ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ewie
    // α=0.15 → half-life ≈ 4.7 fetch cycles (~188s at 40s interval)
    // Reduces jitter while still responding to genuine changes
    if (!m_emaInitialized)
    {
        m_emaSurplus = measuredSurplus;
        m_emaInitialized = true;
    }
    else
    {
        m_emaSurplus = BatteryMonitor::EMA_ALPHA * measuredSurplus + 
                       (1.0f - BatteryMonitor::EMA_ALPHA) * m_emaSurplus;
    }

    // No surplus after smoothing → heater OFF
    if (m_emaSurplus <= 0) {
        LOG_INFO(TAG_BATT, "BLOCKED no surplus raw=%.0f ema=%.0f grid=%.0f batt=%.0f",
                 measuredSurplus, m_emaSurplus, m_gridPower, m_battPower);
        return 0;
    }

    // ━━━ Step 4: Battery priority (SOC threshold) ━━━━━━━━━━━━━━━━━━━━━━━━━
    if (m_akkuPriori > 0 && m_soc > 0)
    {
        if (static_cast<int>(m_soc) < BatteryMonitor::BATT_THRESHOLD) {
            LOG_INFO(TAG_BATT, "BLOCKED soc %d%% < threshold %d%%",
                     (int)m_soc, BatteryMonitor::BATT_THRESHOLD);
            return 0;  // SOC too low → charge battery first
        }
    }

    // ━━━ Step 5: Test bias override ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    int availablePower = static_cast<int>(m_emaSurplus);
    if (m_wattSetupForTest > 0)
    {
        availablePower = m_wattSetupForTest;
        outWattBiasForTest = true;
    }

    return availablePower;
}
