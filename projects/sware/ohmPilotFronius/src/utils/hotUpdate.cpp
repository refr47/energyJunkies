#include "hotUpdate.h"
#include "eprom.h"
#include "app_state.h"
#include <atomic>

static std::atomic<uint32_t> appliedRevision{0};
uint32_t setupAppliedRevision() { return appliedRevision.load(); }

bool setupNeedsRestart(const Setup &a, const Setup &b)
{
    // WiFi manager and energy adapters retain their connection settings.
    if (strcmp(a.ssid, b.ssid) || strcmp(a.passwd, b.passwd)) return true;
#if defined(FRONIUS_IV) || defined(HUAWEI_MODBUS)
    if (strcmp(a.inverter, b.inverter)) return true;
#endif
#ifdef AMIS_READER_DEV
    if (strcmp(a.amisReaderHost, b.amisReaderHost) || strcmp(a.amisKey, b.amisKey)) return true;
#endif
    return false;
}

SetupApplyResult hotUpdate()
{
    if (appliedRevision.load() == eprom_setupRevision()) return SetupApplyResult::Unchanged;
    Setup next;
    uint32_t revision;
    // Flash I/O happens before taking application locks.
    if (!eprom_getSetup(next, &revision)) return SetupApplyResult::Retry;
    if (!appLockData(100)) return SetupApplyResult::Retry;
    if (!appLockConfig(100)) { appUnlockData(); return SetupApplyResult::Retry; }
    if (!appLockPidOut(100)) { appUnlockConfig(); appUnlockData(); return SetupApplyResult::Retry; }
    auto &current = g_app.webSockData.setupData;
    const bool restart = setupNeedsRestart(current, next);
    if (!restart) {
        memcpy(next.currentIP, current.currentIP, sizeof(next.currentIP));
        if (!(next.energyPolling == current.energyPolling)) {
            g_app.webSockData.energyState.valid = false;
            g_app.webSockData.energyState.saving = false;
        }
        next.setupChanged = false;
        current = next;
        appliedRevision.store(revision);
    }
    appUnlockPidOut();
    appUnlockConfig();
    appUnlockData();
    return restart ? SetupApplyResult::RestartRequired : SetupApplyResult::Applied;
}
