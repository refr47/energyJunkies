// =============================================================================
// amisREST.cpp — EnergySource-Adapter für AmisReader
 // =============================================================================

#include "devices/amisREST.h"
#include "debugConsole.h"

// ------------------------------------------------------------------
// init — existierende amisReader_initRestTargets() als Backend
// ------------------------------------------------------------------

bool AmisREST::init(Setup &setupData, WEBSOCK_DATA &webSockData)
{
    (void)setupData;
    bool ok = amisReader_initRestTargets(webSockData);
    if (ok)
    {
        
        LOG_INFO(TAG_AMIS, "AmisReader init OK");
    }
    else
    {
        LOG_ERROR(TAG_AMIS, "AmisReader init failed");
    }
    return ok;
}

// ------------------------------------------------------------------
// read — amisReader_readRestTarget() → unified energyState
// ------------------------------------------------------------------

bool AmisREST::read(WEBSOCK_DATA &webSockData)
{
    if (!amisReader_readRestTarget(webSockData))
    {
        LOG_ERROR(TAG_AMIS, "AmisReader read failed");
        return false;
    }

    auto &src = webSockData.amisReader;
    ENERGY_STATE dst{};

    dst.p_grid   = (double)src.saldo;
    dst.p_load   = (double)src.consumptionInWatt;
    dst.p_pv     = (double)src.exportInWatt;
    dst.soc      = 0.0;
    dst.p_akku   = 0.0;

    if (!publishSample(webSockData, dst)) {
        return false;
    }

    LOG_INFO(TAG_APP_SERVICES,
             "energy (amis): pv=%.0fW grid=%.0fW load=%.0fW",
             dst.p_pv, dst.p_grid, dst.p_load);

    return true;
}
