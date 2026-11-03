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
                           int akkuPriori, int wattSetupForTest,
                           uint32_t sampleSequence, int heaterPowerAtSample)
{
    if (m_akkuPriori != akkuPriori) {
        m_emaSurplus = 0;
        m_emaInitialized = false;
        m_budgetDirty = true;
        m_socReleased = false;
    }
    if (m_sampleSequence != sampleSequence && sampleSequence != 0) {
        m_gridPower = gridPower;
        m_battPower = battPower;
        m_soc = soc;
        m_heaterPowerAtSample = heaterPowerAtSample;
        m_sampleSequence = sampleSequence;
        // Recovery advances only on fresh meter samples, never on control ticks.
        if (m_battPower > BAT_POWER_TRESHOLD) {
            m_dischargeBlocked = true;
            m_clearDischargeSamples = 0;
        } else if (m_dischargeBlocked &&
                   ++m_clearDischargeSamples >= DISCHARGE_RELEASE_SAMPLES) {
            m_dischargeBlocked = false;
        }
        m_budgetDirty = true;
    }
    if (m_soc >= SOC_RELEASE) m_socReleased = true;
    else if (m_soc < SOC_BLOCK) m_socReleased = false;
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
// Returns total heater target power, using heater draw at sample reception.
// Charging is negative; discharging is positive (Fronius convention).
// ──────────────────────────────────────────────────────────────────────────
int BatteryMonitor::getAvailablePower(bool &outWattBiasForTest)
{
    if (!m_budgetDirty) {
        outWattBiasForTest = m_availablePower > 0 && m_wattSetupForTest > 0;
        return outWattBiasForTest ? m_wattSetupForTest : m_availablePower;
    }
    m_budgetDirty = false;
    m_availablePower = 0;
    outWattBiasForTest = false;
    if (m_sampleSequence == 0) {
        return 0;
    }
    const bool chargePriority = m_akkuPriori != BAT_PRIORITY_OFF;

    // Neither priority may heat from the battery. SOC=0 denotes no battery.
    // Reset history so release cannot reuse an old surplus.
    if (m_dischargeBlocked ||
        (chargePriority && m_soc > 0 && !m_socReleased)) {
        m_emaSurplus = 0;
        m_emaInitialized = false;
        LOG_INFO(TAG_BATT, "BLOCKED battery batt=%.0fW soc=%.0f%% priority=%d",
                 m_battPower, m_soc, m_akkuPriori);
        return 0;
    }

    // The grid balance already includes battery charging and heater draw.
    // Add back only the heater draw captured with this measurement. Using
    // current draw here would repeatedly count changes against an old meter value.
    float measuredSurplus = m_heaterPowerAtSample - m_gridPower;
    if (!chargePriority && m_battPower < 0) {
        measuredSurplus -= m_battPower;
    }

    // Advance EMA once per new sample; repeated PID calls return the cached budget.
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
    m_availablePower = static_cast<int>(m_emaSurplus);
    int availablePower = m_availablePower;
    if (m_wattSetupForTest > 0) {
        availablePower = m_wattSetupForTest;
        outWattBiasForTest = true;
    }
    LOG_INFO(TAG_BATT, "AVAILABLE power=%dW (raw=%.0f ema=%.0f grid=%.0f batt=%.0f soc=%.0f%% priority=%d heater=%dW bias=%dW)",
             availablePower, measuredSurplus, m_emaSurplus, m_gridPower,
             m_battPower, m_soc, m_akkuPriori, m_heaterPowerAtSample, m_wattSetupForTest);
    return availablePower;
}
