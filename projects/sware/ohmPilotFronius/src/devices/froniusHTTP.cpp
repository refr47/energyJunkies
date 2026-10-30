// =============================================================================
// froniusHTTP.cpp — EnergySource-Adapter für Fronius Solar Power Flow API
// =============================================================================

#include "devices/froniusHTTP.h"
#include "debugConsole.h"

// ------------------------------------------------------------------
// init — verwendet die existierende soloar_init() als Backend
// ------------------------------------------------------------------

bool FroniusHTTP::init(Setup &setupData, WEBSOCK_DATA &webSockData)
{
    (void)setupData; // soloar_init() liest setup direkt aus g_app.webSockData

    m_akkuAvailable = false;
    bool ok = soloar_init(webSockData);

    if (ok)
    {
        m_akkuAvailable = webSockData.setupData.akku ;
        LOG_INFO(TAG_FRONIUS, "Fronius API OK (akku=%s)",
                 (m_akkuAvailable ? "yes" : "no"));
    }
    else
    {
        LOG_ERROR(TAG_FRONIUS, "Fronius API init failed");
    }

    return ok;
}

// ------------------------------------------------------------------
// read — solar_get_powerflow() → kopiere in energyState
// ------------------------------------------------------------------
 
bool FroniusHTTP::read(WEBSOCK_DATA &webSockData)
{
    if (!solar_get_powerflow(webSockData))
    {
        LOG_ERROR(TAG_FRONIUS, "Fronius API read failed");
        return false;
    }

    // ── fronius_SOLAR_POWERFLOW → unified energyState ──
    auto &src = webSockData.fronius_SOLAR_POWERFLOW;
    ENERGY_STATE dst{};

    dst.p_grid   = src.p_grid;
    dst.p_load   = src.p_load;
    dst.p_pv     = src.p_pv;
    dst.soc      = src.soc;
    dst.p_akku   = src.p_akku;

    if (!publishSample(webSockData, dst)) {
        return false;
    }

    LOG_INFO(TAG_FRONIUS,
             "energy (fronius): pv=%.0fW grid=%.0fW load=%.0fW batt=%.0fW soc=%.1f%%",
             src.p_pv, src.p_grid, src.p_load, src.p_akku, src.soc);

    return true;
}

bool FroniusHTTP::hasBattery() const
{
    return true; // Fronius liefert Batterie-Daten
}
