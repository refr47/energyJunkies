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
    if (m_akkuPriori != akkuPriori) {
        m_emaSurplus = 0;
        m_emaInitialized = false;
    }
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
    return (m_soc <= 0) || (m_battPower <= BAT_POWER_TRESHOLD);
}

// ──────────────────────────────────────────────────────────────────────────
// getAvailablePower — calculate available power for heating
//
// Universal: Works with both AmisReader (battPower=0) and Fronius (battPower≠0)
// Returns total heater target power, including the current heater draw.
// Charging is negative; discharging is positive (Fronius convention).
// ──────────────────────────────────────────────────────────────────────────
int BatteryMonitor::getAvailablePower(int heaterPower, bool &outWattBiasForTest)
{
    outWattBiasForTest = false;
    const bool allowDischarge = m_akkuPriori == BAT_PRIORITY_ALLOW_DISCHARGE;
    const bool chargePriority = m_akkuPriori > BAT_PRIORITY_OFF;

    // Modes 0 and 1 must not heat from the battery. Mode 2 allows it.
    // Reset history so a later release cannot reuse an old surplus.
    if ((!allowDischarge && m_battPower > BAT_POWER_TRESHOLD) ||
        (chargePriority && !allowDischarge && m_soc > 0 &&
         m_soc < BatteryMonitor::BATT_THRESHOLD)) {
        m_emaSurplus = 0;
        m_emaInitialized = false;
        LOG_INFO(TAG_BATT, "BLOCKED battery batt=%.0fW soc=%.0f%% priority=%d",
                 m_battPower, m_soc, m_akkuPriori);
        return 0;
    }

    // The grid balance already includes battery charging and heater draw.
    // Add back the heater draw to obtain a total target, for either grid sign.
    float measuredSurplus = heaterPower - m_gridPower;
    if (!chargePriority && m_battPower < 0) {
        measuredSurplus -= m_battPower;
    }

    // Smooth increases only; stale surplus must not delay a reduction.
    if (!m_emaInitialized || measuredSurplus < m_emaSurplus) {
        m_emaSurplus = measuredSurplus;
        m_emaInitialized = true;
    } else {
        m_emaSurplus = BatteryMonitor::EMA_ALPHA * measuredSurplus +
                       (1.0f - BatteryMonitor::EMA_ALPHA) * m_emaSurplus;
    }

    if (m_emaSurplus <= 0) {
        m_emaSurplus = 0;
        LOG_INFO(TAG_BATT, "BLOCKED no surplus raw=%.0f grid=%.0f batt=%.0f",
                 measuredSurplus, m_gridPower, m_battPower);
        return 0;
    }

    // Test override remains subject to the battery and surplus guards.
    int availablePower = static_cast<int>(m_emaSurplus);
    if (m_wattSetupForTest > 0) {
        availablePower = m_wattSetupForTest;
        outWattBiasForTest = true;
    }
    LOG_INFO(TAG_BATT, "AVAILABLE power=%dW (raw=%.0f ema=%.0f grid=%.0f batt=%.0f soc=%.0f%% priority=%d heater=%dW bias=%dW)",
             availablePower, measuredSurplus, m_emaSurplus, m_gridPower,
             m_battPower, m_soc, m_akkuPriori, heaterPower, m_wattSetupForTest);
    return availablePower;
}
