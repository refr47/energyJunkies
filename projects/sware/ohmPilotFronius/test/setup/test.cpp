#include "setupValidation.h"
#include "eprom.h"
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
    if (expectedErrors) assert(errors.size() >= expectedErrors);
    return ok;
}
int main()
{
    Setup s; setupDefaults(s);
    assert(s.legionellenDelta == 168 && s.phasen_leistung_in_watt == 1500);
    assert(patch(s, "{}"));
    assert(patch(s, R"({"mqtt_server_ip":null,"influx_server_ip":"","influx_token":null})"));
    Setup before = s;
    assert(!patch(s, R"({"mqtt_server_ip":42,"influx_org":[],"pid_epsilon":0,"unknown":true})", 4));
    assert(!memcmp(&before, &s, sizeof(s)));
    assert(!patch(s, R"({"influx_server_ip":"http://localhost:8086"})", 3));
    assert(patch(s, R"({"influx_server_ip":"http://localhost:8086","influx_org":"org","influx_token":"token","influx_bucket":"bucket"})"));
    assert(!patch(s, R"({"influx_server_ip":"http://localhost:99999","mqtt_server_ip":"999.0.0.1"})", 2));
    assert(!patch(s, R"({"mqtt_server_ip":"bad..host"})", 1));
    assert(!patch(s, R"({"akku_vorhanden":2,"heizstab_leistung":-1,"heizstab_temp_min":100})", 3));
    assert(!patch(s, R"({"mqtt_user":"abc\u0000def"})", 1));
    assert(!patch(s, R"({"mqtt_user":"12345678901234567890123456"})", 1));
    assert(patch(s, R"({"mqtt_user":"1234567890123456789012345","mqtt_server_ip":"broker.local"})"));
    assert(patch(s, R"({"mqtt_server_ip":"---","influx_server_ip":null})"));
    assert(!s.mqttSetup.mqttHost[0] && !s.influxSetup.influxHost[0]);
    assert(eprom_storeSetup(s));
    Setup loaded; assert(eprom_getSetup(loaded));
    assert(!strcmp(loaded.mqttSetup.mqttUser, s.mqttSetup.mqttUser));
    Preferences::failWrite = true;
    assert(!eprom_storeSetup(s));
    assert(!eprom_test_write_Eprom("test", "pass"));
    Preferences::failWrite = false;
    Preferences::bytes[offsetof(Setup, akku)] = 255;
    assert(!eprom_getSetup(loaded));
    assert(!strcmp(loaded.ssid, EMPTY_VALUE_IN_SETUP));
    assert(eprom_storeSetup(s));
    memset(Preferences::bytes.data() + offsetof(Setup, ssid), 'x', sizeof(s.ssid));
    assert(!eprom_getSetup(loaded));
    Preferences::failOpen = true;
    assert(!eprom_getSetup(loaded));
    assert(!eprom_storeSetup(s));
    Preferences::failOpen = false;
    assert(eprom_storeSetup(s));
    Preferences::schema = 99;
    assert(!eprom_getSetup(loaded));
    // Logging must not dereference arbitrary addresses or scan beyond char arrays.
    memset(&s, 'x', sizeof(s));
    printEprom(s);
    std::cout << "Setup validation, NVS failures and bounded diagnostics: PASS\n";
}
