#!/usr/bin/env python3
"""Host regression tests of actual service functions with injected lock failures.

Extract functions to avoid linking the ESP hardware/UI dependencies. The tested
function bodies come directly from app_services.cpp, not a duplicated model.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/mainLoop/app_services.cpp').read_text()

def function(signature):
    start = source.index(signature + '\n{')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

preamble = r'''
#include <cassert>
#include <atomic>
#include <memory>
#include <iostream>
#include <string>
using String = std::string;
#define MQTT 1
#define HUAWEI_MODBUS 1
#define LOG_ERROR(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
constexpr int LOCK_TIMEOUT_DATA_MS=50, LOCK_TIMEOUT_CONFIG_MS=50;
constexpr int LOCK_TIMEOUT_PIDOUT_MS=50, LOCK_TIMEOUT_MULTI_MS=100;
bool dataHeld=false, pidHeld=false, configHeld=false;
bool failData=false, failPid=false, failConfig=false, failPublishConfig=false;
bool appLockData(int) { if(failData) return false; assert(!dataHeld); return dataHeld=true; }
void appUnlockData() { assert(dataHeld); dataHeld=false; }
bool appLockPidOut(int) { if(failPid) return false; assert(!pidHeld); return pidHeld=true; }
void appUnlockPidOut() { assert(pidHeld); pidHeld=false; }
bool appLockConfig(int) {
    if(failConfig || (failPublishConfig && dataHeld)) return false;
    assert(!configHeld); return configHeld=true;
}
void appUnlockConfig() { assert(configHeld); configHeld=false; }
struct Setup { bool akku=false; };
struct Data {
    struct { bool networkOK=true, boilerHeating=true, mqtt=true, device=false; } states;
    struct { bool valid=true; } energyState;
    struct { int mAnalogOut=1, PID_PIN1=1, PID_PIN2=1; } pidContainer;
    Setup setupData;
};
struct App {
    Data webSockData;
    struct { int resets=0; void reset() { assert(pidHeld); ++resets; } } pinManager;
} g_app;
void ledHandler_showNetworkError(bool) {}
bool wifiUp=true;
bool wifi_isStillConnected(Setup&) { return wifiUp; }
int inits=0;
bool initOK=true, readOK=false;
struct EnergySource {
    virtual ~EnergySource() = default;
    bool init(Setup&, Data&) { assert(!dataHeld && !configHeld && !pidHeld); ++inits; return initOK; }
    bool read(Data& data) { if (readOK) data.energyState.valid=true; return readOK; }
    const char* name() const { return "fake"; }
    bool hasBattery() const { return true; }
    int type() const { return 1; }
};
struct HuaweiModbus : EnergySource {};
std::atomic<EnergySource*> g_activeSource{nullptr};
int g_sourceType=0;
static void handleLockFailure(const char*);
'''
names = [
    'static void stopHeating(const char* reason)',
    'static void markNetworkDown(const char* reason)',
    'static void handleLockFailure(const char* context)',
    'static void tryMarkNetworkDown(const char* reason, const char* failMsg)',
    'void serviceNetworkSupervisor()',
    'static bool ensureEnergySource()',
    'static void markEnergyUnavailable(const char* reason)',
    'static bool fetchEnergy()',
]
tests = r'''
void unlocked() { assert(!dataHeld && !pidHeld && !configHeld); }
int main() {
    // Data acquired, PID lock fails: no leaked data lock, no unlocked reset.
    failPid=true;
    tryMarkNetworkDown("test", "test");
    unlocked(); assert(g_app.pinManager.resets==0);
    failPid=false;
    failData=true;
    tryMarkNetworkDown("test", "test");
    unlocked(); assert(g_app.pinManager.resets==1);
    failData=false;

    // Busy/read failure disables heating and measurements, not WLAN/MQTT.
    markEnergyUnavailable("busy");
    unlocked();
    assert(!g_app.webSockData.energyState.valid);
    assert(!g_app.webSockData.states.boilerHeating);
    assert(g_app.webSockData.states.networkOK && g_app.webSockData.states.mqtt);

    // Real WLAN loss still disables dependent operation.
    wifiUp=false;
    serviceNetworkSupervisor();
    unlocked();
    assert(!g_app.webSockData.states.networkOK && !g_app.webSockData.states.mqtt);
    wifiUp=true;
    serviceNetworkSupervisor();
    assert(g_app.webSockData.states.networkOK && !g_app.webSockData.states.mqtt);
    assert(inits==0);

    // Failed configuration lock must not initialize with invalid setup.
    failConfig=true;
    assert(!ensureEnergySource());
    assert(inits==0); unlocked();
    failConfig=false;
    initOK=false;
    assert(!ensureEnergySource());
    assert(inits==1 && !g_activeSource.load()); unlocked();
    initOK=true;

    // Successful init survives a failed publication lock, without reinit.
    failPublishConfig=true;
    assert(!ensureEnergySource());
    assert(inits==2 && !g_activeSource.load()); unlocked();
    failPublishConfig=false;
    assert(ensureEnergySource());
    EnergySource* original=g_activeSource.load();
    assert(original && inits==2); unlocked();
    assert(g_app.webSockData.setupData.akku && g_app.webSockData.states.device);

    // Neither supervisor lock contention nor read errors reinitialize or
    // discard the already selected adapter.
    failData=true;
    serviceNetworkSupervisor();
    failData=false;
    g_app.webSockData.states.mqtt=true;
    assert(!fetchEnergy());
    assert(!g_app.webSockData.energyState.valid);
    assert(g_app.webSockData.states.networkOK && g_app.webSockData.states.mqtt);
    assert(ensureEnergySource());
    readOK=true;
    assert(fetchEnergy() && g_app.webSockData.energyState.valid);
    assert(inits==2 && original==g_activeSource.load()); unlocked();
    delete g_activeSource.exchange(nullptr);
    std::cout << "Energy service regressions passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix='sware-energy-service-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(preamble + '\n'.join(map(function, names)) + tests)
    binary = Path(tmp) / 'test'
    subprocess.run(['g++', '-std=c++14', '-Wall', '-Wextra', '-Wno-unused-parameter',
                    '-Werror', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
