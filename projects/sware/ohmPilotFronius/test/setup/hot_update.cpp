#include "setupValidation.h"
#include "hotUpdate.h"
#include "eprom.h"
#include "app_state.h"
#include <cassert>
#include <iostream>

int main()
{
    Setup setup; setupDefaults(setup);
    g_app.webSockData.setupData = setup;
    uint32_t revision;
    assert(eprom_storeSetup(setup, &revision));
    assert(hotUpdate() == SetupApplyResult::Applied);
    assert(setupAppliedRevision() == revision);
    assert(hotUpdate() == SetupApplyResult::Unchanged);
    setup.energyPolling.daySeconds = 15;
    setup.mqttEnabled = true;
    strcpy(setup.mqttSetup.mqttHost, "new-broker");
    setup.mqttPort = 1884;
    assert(eprom_storeSetup(setup, &revision));
    g_app.webSockData.energyState.valid = true;
    g_app.webSockData.energyState.saving = true;
    failConfig = true;
    assert(hotUpdate() == SetupApplyResult::Retry && !dataLocked && !configLocked);
    assert(setupAppliedRevision() != revision);
    failConfig = false; failPid = true;
    assert(hotUpdate() == SetupApplyResult::Retry && !dataLocked && !configLocked && !pidLocked);
    failPid = false;
    assert(hotUpdate() == SetupApplyResult::Applied);
    assert(setupAppliedRevision() == revision);
    assert(!g_app.webSockData.energyState.valid && !g_app.webSockData.energyState.saving);
    assert(g_app.webSockData.setupData.mqttPort == 1884);
    // A second save before application must not be lost behind a boolean flag.
    setup.energyPolling.daySeconds = 20; assert(eprom_storeSetup(setup));
    setup.energyPolling.daySeconds = 25; assert(eprom_storeSetup(setup, &revision));
    assert(hotUpdate() == SetupApplyResult::Applied && setupAppliedRevision() == revision);
    assert(g_app.webSockData.setupData.energyPolling.daySeconds == 25);
    strcpy(setup.ssid, "changed-wifi");
    assert(eprom_storeSetup(setup, &revision));
    assert(hotUpdate() == SetupApplyResult::RestartRequired);
    assert(setupAppliedRevision() != revision);
    assert(!dataLocked && !configLocked && !pidLocked);
    std::cout << "Hotupdate revisions, application and lock-failure cleanup: PASS\n";
}
