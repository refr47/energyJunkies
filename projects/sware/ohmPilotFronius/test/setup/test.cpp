#include "setupValidation.h"
#include "eprom.h"
#include "setupLegacy.h"
#include <Preferences.h>
#include <cassert>
#include <limits>
#include <iostream>

static bool patch(Setup &s, const char *input, size_t expectedErrors = 0)
{
    JsonDocument json, result;
    assert(!deserializeJson(json, input));
    JsonArray errors = result.to<JsonArray>();
    const bool ok = applySetupPatch(s, json.as<JsonObject>(), errors);
    if (expectedErrors && errors.size() < expectedErrors) { std::cerr << input << "\n"; serializeJson(result, std::cerr); std::cerr << "\n"; assert(false); }
    return ok;
}
int main()
{
    Setup s; setupDefaults(s);
    assert(s.legionellenDelta == 168 && s.phasen_leistung_in_watt == 1500);
    assert(s.akkuPriori == 1);
    assert(!s.gridDemand.enabled && !s.gridDemand.enforceLimit && s.gridDemand.limitW == 10000);
    assert(!patch(s, R"({"grid_demand_limit_w":0})", 1));
    assert(patch(s, R"({"grid_demand_enabled":true,"grid_demand_enforce_limit":true,"grid_demand_limit_w":9000})"));
    assert(!patch(s, R"({"akku_priori":2})", 1));
    assert(patch(s, R"({"akku_priori":0})"));
    assert(patch(s, R"({"akku_priori":1})"));
    assert(patch(s, "{}"));
    assert(patch(s, R"({"mqtt_server_ip":null,"influx_server_ip":"","influx_token":null})"));
    Setup before = s;
    assert(!patch(s, R"({"mqtt_server_ip":42,"influx_org":[],"pid_epsilon":0,"unknown":true})", 4));
    assert(!memcmp(&before, &s, sizeof(s)));
    assert(!patch(s, R"({"influx_enabled":true,"influx_server_ip":"http://localhost:8086"})", 3));
    assert(patch(s, R"({"influx_enabled":true,"influx_server_ip":"http://localhost:8086","influx_org":"org","influx_token":"token","influx_bucket":"bucket"})"));
    assert(!patch(s, R"({"mqtt_enabled":true,"influx_server_ip":"http://localhost:99999","mqtt_server_ip":"999.0.0.1"})", 2));
    assert(!patch(s, R"({"mqtt_enabled":true,"mqtt_server_ip":"bad..host"})", 1));
    assert(!patch(s, R"({"akku_vorhanden":2,"heizstab_leistung":-1,"heizstab_temp_min":100})", 3));
    assert(!patch(s, R"({"mqtt_user":"abc\u0000def"})", 1));
    assert(!patch(s, R"({"mqtt_user":"12345678901234567890123456"})", 1));
    assert(patch(s, R"({"mqtt_user":"1234567890123456789012345","mqtt_server_ip":"broker.local"})"));
    assert(patch(s, R"({"mqtt_enabled":false,"influx_enabled":false,"mqtt_server_ip":"---","influx_server_ip":null})"));
    assert(!s.mqttSetup.mqttHost[0] && !s.influxSetup.influxHost[0]);
    assert(eprom_storeSetup(s));
    Setup loaded; assert(eprom_getSetup(loaded));
    assert(!strcmp(loaded.mqttSetup.mqttUser, s.mqttSetup.mqttUser));
    Preferences::failWrite = true;
    assert(!eprom_storeSetup(s));
    assert(!eprom_test_write_Eprom("test", "pass"));
    Preferences::failWrite = false;
    // The test helper writes a complete default snapshot with the supplied
    // WLAN credentials; it does not generate data inside eprom_storeSetup().
    assert(eprom_test_write_Eprom("fixture-wifi", "fixture-password"));
    assert(eprom_getSetup(loaded));
    assert(!strcmp(loaded.ssid, "fixture-wifi"));
    assert(!strcmp(loaded.passwd, "fixture-password"));
    assert(loaded.tempMinInGrad == 5);
    assert(eprom_storeSetup(s));
    auto saved = Preferences::blobs["setup_v2"];
    JsonDocument oldDemand;
    assert(!deserializeJson(oldDemand, saved.data(), saved.size()));
    oldDemand["settings"].remove("grid_demand_enabled");
    oldDemand["settings"].remove("grid_demand_enforce_limit");
    oldDemand["settings"].remove("grid_demand_limit_w");
    std::string oldPayload;
    serializeJson(oldDemand, oldPayload);
    Preferences::blobs["setup_v2"].assign(oldPayload.begin(), oldPayload.end());
    assert(eprom_getSetup(loaded) && !loaded.gridDemand.enabled && loaded.gridDemand.limitW == 10000);
    Preferences::blobs["setup_v2"] = saved;
    assert(eprom_getSetup(loaded) && loaded.gridDemand.enabled && loaded.gridDemand.enforceLimit && loaded.gridDemand.limitW == 9000);
    JsonDocument retired;
    assert(!deserializeJson(retired, saved.data(), saved.size()));
    retired["settings"]["akku_priori"] = 2;
    std::string retiredPayload;
    serializeJson(retired, retiredPayload);
    Preferences::blobs["setup_v2"].assign(retiredPayload.begin(), retiredPayload.end());
    assert(eprom_getSetup(loaded) && loaded.akkuPriori == 1);
    Preferences::blobs["setup_v2"] = saved;
    Preferences::blobs["setup_v2"][0] = '!';
    assert(!eprom_getSetup(loaded));
    assert(!strcmp(loaded.ssid, EMPTY_VALUE_IN_SETUP));
    Preferences::blobs["setup_v2"] = saved;
    Preferences::failOpen = true;
    assert(!eprom_getSetup(loaded));
    assert(!eprom_storeSetup(s));
    Preferences::failOpen = false;
    assert(eprom_storeSetup(s));
    assert(!patch(s, R"({"energy_day_seconds":0,"energy_saving_seconds":1,"energy_night_start":24,"energy_hold_seconds":0,"energy_low_pv_watts":50,"energy_wake_pv_watts":20,"mqtt_port":0})", 6));
    assert(patch(s, R"({"mqtt_enabled":true,"mqtt_server_ip":"broker.local","mqtt_port":1884,"energy_day_seconds":15,"energy_saving_seconds":60,"energy_night_enabled":false})"));
    uint32_t first, second;
    assert(eprom_storeSetup(s, &first));
    assert(eprom_getSetup(loaded, &second) && first == second);
    assert(loaded.mqttEnabled && loaded.mqttPort == 1884 && loaded.energyPolling.daySeconds == 15);
    assert(patch(s, R"({"mqtt_enabled":false})"));
    assert(!strcmp(s.mqttSetup.mqttHost, "broker.local"));
    assert(eprom_storeSetup(s, &second) && second > first);
    // Legacy migration preserves old settings; adding fields never requires erasing NVS.
    LegacySetupV1 old{};
    strcpy(old.ssid, "legacy-wifi"); strcpy(old.passwd, "legacy-password");
    strcpy(old.mqttSetup.mqttHost, "broker.local");
    strcpy(old.mqttSetup.mqttUser, "legacy-user");
    old.heizstab_leistung_in_watt = 3000;
    old.tempMinInGrad = 10; old.tempMaxAllowedInGrad = 70;
    old.legionellenDelta = 168; old.legionellenMaxTemp = 65;
    old.epsilonML_PinManager = 0.1;
    old.akkuPriori = 2;
    Preferences::blobs.clear(); Preferences::magic = 0xDEADBEEF; Preferences::schema = 1;
    auto *bytes = reinterpret_cast<unsigned char *>(&old);
    Preferences::blobs["setup_block"].assign(bytes, bytes + sizeof(old));
    const auto legacy = Preferences::blobs["setup_block"];
    assert(eprom_getSetup(loaded, &first) && first == 1);
    assert(!strcmp(loaded.ssid, "legacy-wifi") && loaded.mqttEnabled && loaded.mqttPort == 1883);
    assert(loaded.akkuPriori == 1);
    assert(!loaded.influxEnabled && loaded.energyPolling.daySeconds == 40);
    assert(eprom_storeSetup(loaded, &second) && second == 2);
    assert(Preferences::blobs["setup_block"] == legacy);
    assert(eprom_getSetup(loaded) && !strcmp(loaded.mqttSetup.mqttUser, "legacy-user"));
    Preferences::blobs.erase("setup_v2");
    Preferences::blobs["setup_block"][offsetof(LegacySetupV1, akku)] = 255;
    assert(!eprom_getSetup(loaded));
    Preferences::blobs["setup_block"] = legacy;
    memset(Preferences::blobs["setup_block"].data() + offsetof(LegacySetupV1, ssid), 'x', sizeof(old.ssid));
    assert(!eprom_getSetup(loaded));
    // Logging must not dereference arbitrary addresses or scan beyond char arrays.
    memset(static_cast<void *>(&s), 'x', sizeof(s));
    printEprom(s);
    std::cout << "Setup validation, NVS failures and bounded diagnostics: PASS\n";
}
