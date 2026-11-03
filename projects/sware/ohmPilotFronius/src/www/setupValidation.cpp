#include "setupValidation.h"
#include "ajaxConst.h"
#include <cmath>
#include <cctype>
#include <climits>

static void error(JsonArray errors, const char *field, const char *message)
{
    JsonObject entry = errors.add<JsonObject>();
    entry["field"] = field;
    entry["message"] = message;
}

void setupDefaults(Setup &s)
{
    s = Setup{};
    s.akkuPriori = 1; // Preserve the evening reserve by default.
    strlcpy(s.ssid, EMPTY_VALUE_IN_SETUP, sizeof(s.ssid));
    s.heizstab_leistung_in_watt = 4500;
    s.phasen_leistung_in_watt = 1500;
    s.tempMaxAllowedInGrad = 80;
    s.tempMinInGrad = 5;
    s.legionellenDelta = 168; // hours, as consumed by thermalController
    s.legionellenMaxTemp = 70;
    s.epsilonML_PinManager = 0.05;
    s.mqttPort = 1883;
    s.energyPolling = EnergyPollingConfig{};
}

static bool optional(const char *key)
{
    return !strncmp(key, "mqtt_", 5) || !strncmp(key, "influx_", 7);
}

static bool hostValid(const char *host, size_t n)
{
    if (!n) return false;
    size_t label = 0;
    bool numeric = true;
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = host[i];
        if (c == '.') {
            if (!label || host[i-1] == '-') return false;
            label = 0;
        } else {
            if (!isalnum(c) && c != '-') return false;
            if (!label && c == '-') return false;
            if (++label > 63) return false;
            if (!isdigit(c)) numeric = false;
        }
    }
    if (!label || host[n-1] == '-') return false;
    if (numeric) {
        unsigned octet = 0, dots = 0;
        for (size_t i = 0; i < n; ++i) {
            if (host[i] == '.') { ++dots; octet = 0; }
            else { octet = octet * 10 + host[i] - '0'; if (octet > 255) return false; }
        }
        if (dots != 3) return false;
    }
    return true;
}

static bool urlValid(const char *url)
{
    const char *host = !strncmp(url, "http://", 7) ? url + 7 :
                       !strncmp(url, "https://", 8) ? url + 8 : nullptr;
    if (!host) return false;
    const char *end = host + strcspn(host, ":/");
    if (!hostValid(host, end - host)) return false;
    if (*end == ':') {
        const char *port = ++end;
        unsigned value = 0;
        while (isdigit(static_cast<unsigned char>(*end))) {
            value = value * 10 + (*end++ - '0');
            if (value > 65535) return false;
        }
        if (end == port || value == 0) return false;
    }
    if (*end && *end != '/') return false;
    for (; *end; ++end)
        if (isspace(static_cast<unsigned char>(*end)) || static_cast<unsigned char>(*end) < 32) return false;
    return true;
}

bool validateSetup(Setup &s, JsonArray errors)
{
    const size_t before = errors.size();
    // Check every array before any strlen/strcmp or serializer touches NVS bytes.
#define CHECK(member, key) if (!memchr(s.member, 0, sizeof(s.member))) error(errors, key, "Text ist nicht nullterminiert")
    CHECK(ssid, WLAN_ESSID); CHECK(passwd, WLAN_PASSWD);
    CHECK(inverter, IP_INVERTER); CHECK(currentIP, "currentIP");
    CHECK(amisReaderHost, AMIS_READER_HOST); CHECK(amisKey, AMIS_READER_KEY);
    CHECK(mqttSetup.mqttHost, WWW_MQTT_HOST); CHECK(mqttSetup.mqttUser, WWW_MQTT_USER); CHECK(mqttSetup.mqttPass, WWW_MQTT_PASWWD);
    CHECK(influxSetup.influxHost, WWW_INFLUX_HOST); CHECK(influxSetup.influxOrg, WWW_INFLUX_ORG);
    CHECK(influxSetup.influxToken, WWW_INFLUX_TOKEN); CHECK(influxSetup.influxBucket, WWW_INFLUX_BUCKET);
#undef CHECK
    if (errors.size() != before) return false;
    // Legacy disabled-host marker is accepted and normalized.
    if (!strcmp(s.mqttSetup.mqttHost, EMPTY_VALUE_IN_SETUP)) s.mqttSetup.mqttHost[0] = 0;
    if (!strcmp(s.influxSetup.influxHost, EMPTY_VALUE_IN_SETUP)) s.influxSetup.influxHost[0] = 0;
    if (s.mqttEnabled && !hostValid(s.mqttSetup.mqttHost, strlen(s.mqttSetup.mqttHost)))
        error(errors, WWW_MQTT_HOST, "Hostname oder IPv4-Adresse ohne Protokoll/Port erwartet");
    if (s.influxEnabled) {
        if (!urlValid(s.influxSetup.influxHost)) error(errors, WWW_INFLUX_HOST, "HTTP(S)-URL mit gueltigem Host und optionalem Port erwartet");
        if (!s.influxSetup.influxOrg[0]) error(errors, WWW_INFLUX_ORG, "Bei aktiviertem Influx erforderlich");
        if (!s.influxSetup.influxToken[0]) error(errors, WWW_INFLUX_TOKEN, "Bei aktiviertem Influx erforderlich");
        if (!s.influxSetup.influxBucket[0]) error(errors, WWW_INFLUX_BUCKET, "Bei aktiviertem Influx erforderlich");
    }
    if (!s.mqttPort) error(errors, "mqtt_port", "Port muss zwischen 1 und 65535 liegen");
    const auto &c = s.energyPolling;
    if (c.daySeconds < 5 || c.daySeconds > 3600) error(errors, "energy_day_seconds", "Erlaubt sind 5 bis 3600 Sekunden");
    if (c.savingSeconds < 5 || c.savingSeconds < c.daySeconds || c.savingSeconds > 86400) error(errors, "energy_saving_seconds", "Mindestens Normalintervall, maximal 86400 Sekunden");
    if (c.nightStart > 23) error(errors, "energy_night_start", "Stunde muss zwischen 0 und 23 liegen");
    if (c.nightEnd > 23) error(errors, "energy_night_end", "Stunde muss zwischen 0 und 23 liegen");
    if (!std::isfinite(c.lowPvWatts) || c.lowPvWatts < 0 || c.lowPvWatts > 1000000) error(errors, "energy_low_pv_watts", "Erlaubt sind 0 bis 1000000 Watt");
    if (!std::isfinite(c.wakePvWatts) || c.wakePvWatts <= c.lowPvWatts || c.wakePvWatts > 1000000) error(errors, "energy_wake_pv_watts", "Groesser als Sparschwelle, maximal 1000000 Watt");
    if (c.holdSeconds < 1 || c.holdSeconds > 86400) error(errors, "energy_hold_seconds", "Erlaubt sind 1 bis 86400 Sekunden");
    if (!s.heizstab_leistung_in_watt || s.heizstab_leistung_in_watt > INT_MAX)
        error(errors, HEIZSTABLEISTUNG, "Positive Leistung im unterstuetzten Ganzzahlbereich erwartet");
    if (s.akkuPriori < 0 || s.akkuPriori > 1) error(errors, AKKU_PRIORI, "Erlaubt sind 0 und 1");
    if (s.tempMinInGrad > s.tempMaxAllowedInGrad) error(errors, TEMP_EINSCHALT, "Mindesttemperatur darf Maximaltemperatur nicht uebersteigen");
    if (!std::isfinite(s.epsilonML_PinManager) || s.epsilonML_PinManager <= 0) error(errors, PID_EPSILON, "Endliche Zahl groesser als 0 erwartet");
    if (s.forceHeating != 0 && s.forceHeating != 1 && s.forceHeating != 2 && s.forceHeating != 3 && s.forceHeating != 10)
        error(errors, FORCE_HEIZPATRONE, "Erlaubt sind 0, 1, 2, 3 und 10");
    if (s.forceHeatingStop != 0 && s.forceHeatingStop != 1) error(errors, FORCE_HEIZPATRONE_STOP, "Erlaubt sind 0 und 1");
    s.phasen_leistung_in_watt = s.heizstab_leistung_in_watt / 3;
    return errors.size() == before;
}

bool applySetupPatch(Setup &s, JsonObject patch, JsonArray errors)
{
    Setup candidate = s;
    for (JsonPair pair : patch) {
        const FieldBase<Setup> *field = nullptr;
        for (size_t i = 0; i < setupFieldsCount; ++i)
            if (!strcmp(pair.key().c_str(), setupFields[i]->getKey())) { field = setupFields[i]; break; }
        if (!field) { error(errors, pair.key().c_str(), "Unbekanntes Feld"); continue; }
        // For optional text fields null explicitly clears the value; omission preserves it.
        if (optional(pair.key().c_str()) && field->maxLength() && pair.value().isNull()) pair.value().set("");
        if (!field->update(candidate, pair.value())) {
            error(errors, field->getKey(), field->validationMessage());
            if (field->maxLength()) errors[errors.size()-1]["maxLength"] = field->maxLength();
        }
    }
    validateSetup(candidate, errors);
    if (!errors.size()) { s = candidate; return true; }
    return false;
}
