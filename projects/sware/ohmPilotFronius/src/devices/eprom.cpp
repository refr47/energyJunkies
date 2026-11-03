#define __EEPROM_CPP

#include <Preferences.h>
#include <atomic>
#include <mutex>

#include "utils.h"
#include "eprom.h"
#include "setupValidation.h"
#include "ajaxConst.h"
#include "setupLegacy.h"
#include <memory>

#define SETUP_CREDENTIALS "setup"
#define NVS_BLOCK_MAGIC_KEY "blk_magic"
#define NVS_BLOCK_MAGIC_VAL 0xDEADBEEF
#define SHELLY_EPROM "shelly"
#define SHELLY_DEVICE_NAME "sdn"
#define SHELLY_MAC "smac"
#define SHELLY_IP "sip"
#define SHELLY_PORT "spt"
#define SHELLY_COUNT_KEY "scnt"

// #include "tft.h"

// https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/#example2

static Preferences preferences;

// -------------------------------------------------------------------
// THREAD-SAFETY: atomic statt freiem bool (ESP32 dual-core Data-Race!)
// -------------------------------------------------------------------
static std::mutex s_epromMutex;
static std::atomic<bool> s_stammDataUpdateWatch{false};

bool eprom_stammDataUpdate()
{
    return s_stammDataUpdateWatch.load();
}
void eprom_stammDataUpdateReset()
{
    s_stammDataUpdateWatch.store(false);
}

static std::atomic<uint32_t> setupRevision{0};
uint32_t eprom_setupRevision() { return setupRevision.load(); }

bool eprom_storeSetup(Setup &setup, uint32_t *savedRevision)
{
    JsonDocument doc;
    if (!validateSetup(setup, doc.to<JsonArray>()) || doc.overflowed()) return false;
    std::lock_guard<std::mutex> lock(s_epromMutex);
    doc.clear();
    doc["schema"] = 2;
    uint32_t revision = setupRevision.load() + 1;
    if (!revision) revision = 1;
    doc["revision"] = revision;
    JsonObject fields = doc["settings"].to<JsonObject>();
    for (size_t i = 0; i < setupFieldsCount; ++i) setupFields[i]->serialize(setup, fields);
    if (doc.overflowed()) return false;
    String payload;
    const size_t bytes = serializeJson(doc, payload);
    if (!bytes || bytes != measureJson(doc)) return false;
    if (!preferences.begin(SETUP_CREDENTIALS, false)) return false;
    // One NVS blob contains schema, revision and settings; legacy bytes stay intact.
    std::unique_ptr<char[]> verify(new (std::nothrow) char[bytes]);
    if (!verify) { preferences.end(); return false; }
    bool ok = preferences.putBytes("setup_v2", payload.c_str(), bytes) == bytes;
    ok = ok && verify && preferences.getBytes("setup_v2", verify.get(), bytes) == bytes
        && memcmp(payload.c_str(), verify.get(), bytes) == 0;
    preferences.end();
    if (ok) {
        setupRevision.store(revision);
        if (savedRevision) *savedRevision = revision;
        s_stammDataUpdateWatch.store(true);
    }
    return ok;
}

void eprom_isInit()
{
    std::lock_guard<std::mutex> lock(s_epromMutex);
    if (preferences.begin(SETUP_CREDENTIALS, true)) preferences.end();
}

bool eprom_getSetup(Setup &setup, uint32_t *revision)
{
    setupDefaults(setup);
    if (revision) *revision = 0;
    std::lock_guard<std::mutex> lock(s_epromMutex);
    if (!preferences.begin(SETUP_CREDENTIALS, true)) return false;
    const size_t size = preferences.getBytesLength("setup_v2");
    if (size) {
        if (size > 16384) { preferences.end(); return false; }
        std::unique_ptr<char[]> buffer(new (std::nothrow) char[size]);
        const bool read = buffer && preferences.getBytes("setup_v2", buffer.get(), size) == size;
        preferences.end();
        if (!read) return false;
        JsonDocument doc;
        if (deserializeJson(doc, static_cast<const char *>(buffer.get()), size)
            || doc["schema"] != 2 || !doc["revision"].is<uint32_t>()
            || doc["revision"].as<uint32_t>() == 0 || !doc["settings"].is<JsonObject>()) return false;
        JsonObject fields = doc["settings"];
        // Stored documents are full snapshots, not partial HTTP patches.
        for (size_t i = 0; i < setupFieldsCount; ++i)
            if (fields[setupFields[i]->getKey()].isNull()) return false;
        // Retired discharge mode migrates to battery priority on load only.
        if (fields[AKKU_PRIORI].is<int>() && fields[AKKU_PRIORI].as<int>() == 2)
            fields[AKKU_PRIORI] = 1;
        JsonDocument errors;
        Setup candidate;
        setupDefaults(candidate);
        if (!applySetupPatch(candidate, fields, errors.to<JsonArray>()) || errors.overflowed()) return false;
        setup = candidate;
        const uint32_t loadedRevision = doc["revision"];
        setupRevision.store(loadedRevision);
        if (revision) *revision = loadedRevision;
        return true;
    }
    LegacySetupV1 old{};
    const bool headerOK = preferences.getUInt("schema", 0) <= 1
        && preferences.getUInt(NVS_BLOCK_MAGIC_KEY, 0) == NVS_BLOCK_MAGIC_VAL
        && preferences.getBytesLength("setup_block") == sizeof(old);
    const bool read = headerOK && preferences.getBytes("setup_block", &old, sizeof(old)) == sizeof(old);
    preferences.end();
    if (!read) return false;
    const auto *raw = reinterpret_cast<const unsigned char *>(&old);
    if (raw[offsetof(LegacySetupV1, akku)] > 1) return false;
    Setup candidate;
    setupDefaults(candidate);
    memcpy(candidate.ssid, old.ssid, sizeof(candidate.ssid));
    memcpy(candidate.passwd, old.passwd, sizeof(candidate.passwd));
    candidate.heizstab_leistung_in_watt = old.heizstab_leistung_in_watt;
    candidate.phasen_leistung_in_watt = old.phasen_leistung_in_watt;
    candidate.tempMaxAllowedInGrad = old.tempMaxAllowedInGrad;
    candidate.tempMinInGrad = old.tempMinInGrad;
    memcpy(candidate.inverter, old.inverter, sizeof(candidate.inverter));
    memcpy(candidate.currentIP, old.currentIP, sizeof(candidate.currentIP));
    candidate.akku = old.akku;
    candidate.akkuPriori = old.akkuPriori == 2 ? 1 : old.akkuPriori;
    candidate.legionellenDelta = old.legionellenDelta;
    candidate.legionellenMaxTemp = old.legionellenMaxTemp;
    memcpy(candidate.amisKey, old.amisKey, sizeof(candidate.amisKey));
    memcpy(candidate.amisReaderHost, old.amisReaderHost, sizeof(candidate.amisReaderHost));
    candidate.epsilonML_PinManager = old.epsilonML_PinManager;
    candidate.forceHeating = old.forceHeating;
    candidate.forceHeatingStop = old.forceHeatingStop;
    candidate.wattSetupForTest = old.wattSetupForTest;
    memcpy(&candidate.mqttSetup, &old.mqttSetup, sizeof(candidate.mqttSetup));
    memcpy(&candidate.influxSetup, &old.influxSetup, sizeof(candidate.influxSetup));
    candidate.mqttEnabled = candidate.mqttSetup.mqttHost[0] && strncmp(candidate.mqttSetup.mqttHost, "---", sizeof(candidate.mqttSetup.mqttHost));
    candidate.influxEnabled = candidate.influxSetup.influxHost[0] && strncmp(candidate.influxSetup.influxHost, "---", sizeof(candidate.influxSetup.influxHost));
    JsonDocument errors;
    if (!validateSetup(candidate, errors.to<JsonArray>()) || errors.overflowed()) return false;
    setup = candidate;
    setupRevision.store(1);
    if (revision) *revision = 1;
    // Migration is non-destructive; next successful save writes schema 2.
    return true;
}

bool eprom_test_write_Eprom(const char *wlanE, const char *passW)
{
    Setup setup;
    setupDefaults(setup);
    if (!wlanE || !passW || strlen(wlanE) >= sizeof(setup.ssid) || strlen(passW) >= sizeof(setup.passwd)) return false;
    strlcpy(setup.ssid, wlanE, sizeof(setup.ssid));
    strlcpy(setup.passwd, passW, sizeof(setup.passwd));
    return eprom_storeSetup(setup);
}

void printEprom(Setup &setup)
{
    // Bounded source reads remain safe even for damaged, unterminated NVS strings.
    LOG_DEBUG(TAG_EPPROM, "Setup: WLAN=%.*s inverter=%.*s MQTT=%.*s Influx=%.*s",
        (int)sizeof(setup.ssid), setup.ssid, (int)sizeof(setup.inverter), setup.inverter,
        (int)sizeof(setup.mqttSetup.mqttHost), setup.mqttSetup.mqttHost,
        (int)sizeof(setup.influxSetup.influxHost), setup.influxSetup.influxHost);
    LOG_DEBUG(TAG_EPPROM, "Setup: heater=%u min=%u max=%u; credentials omitted",
        setup.heizstab_leistung_in_watt, setup.tempMinInGrad, setup.tempMaxAllowedInGrad);
}

void eprom_test_read_Eprom()
{
    Setup setup;
    eprom_getSetup(setup);
    printEprom(setup);
}

void eprom_show(Setup &setup)
{
    printEprom(setup);
}
void eprom_initDefaults()
{
    LOG_ERROR(TAG_EPPROM,"eprom_initDefaults: NVS corrupt or empty — writing defaults");
    Setup setup;
    setupDefaults(setup);
    if (!eprom_storeSetup(setup)) LOG_ERROR(TAG_EPPROM, "Cannot persist defaults");
}

/* *************************** SHELLY */

void eprom_store_shelly(ALL_SHELLY_DEVICES *allDevices, unsigned upperLimit)
{
    LOG_INFO(TAG_EPPROM,"eprom::eprom_store_shelly\n");

    std::lock_guard<std::mutex> lock(s_epromMutex);

    preferences.begin(SHELLY_EPROM, false);

    // H-1 FIX: remove only stale suffixed keys above new upperLimit (instead of destructive clear())
    unsigned int prevCount = preferences.getUInt(SHELLY_COUNT_KEY, 0);
    char key[16];
    for (unsigned int i = upperLimit; i < prevCount; i++)
    {
        sprintf(key, "%s_%d", SHELLY_DEVICE_NAME, i);
        preferences.remove(key);
        sprintf(key, "%s_%d", SHELLY_MAC, i);
        preferences.remove(key);
        sprintf(key, "%s_%d", SHELLY_IP, i);
        preferences.remove(key);
        sprintf(key, "%s_%d", SHELLY_PORT, i);
        preferences.remove(key);
    }

    for (int i = 0; i < upperLimit; i++)
    {
        if (allDevices[i].valid == true)
        {
            if (allDevices[i].errorContainer == NULL)
            {
                // H-1 FIX: suffix keys with device index to prevent NVS collision
                char key[16];
                LOG_INFO(TAG_EPPROM,"eprom:: index: %d, device name: %s, mac: %s, ip: %s, port: %d\n", i, allDevices[i].shellyDevice->name, allDevices[i].shellyDevice->mac, allDevices[i].shellyDevice->ip, allDevices[i].shellyDevice->port);
                sprintf(key, "%s_%d", SHELLY_DEVICE_NAME, i);
                preferences.putString(key, allDevices[i].shellyDevice->name);
                sprintf(key, "%s_%d", SHELLY_MAC, i);
                preferences.putString(key, allDevices[i].shellyDevice->mac);
                sprintf(key, "%s_%d", SHELLY_IP, i);
                preferences.putString(key, allDevices[i].shellyDevice->ip);
                sprintf(key, "%s_%d", SHELLY_PORT, i);
                preferences.putUInt(key, allDevices[i].shellyDevice->port);
                free(allDevices[i].shellyDevice);
            }
            else
            {
                LOG_ERROR(TAG_EPPROM,"eprom::eprom_store_shelly() - ERROR Msg: %s, Method: %s\n", allDevices[i].errorContainer->errorMessage, allDevices[i].errorContainer->usedMethod);
                free(allDevices[i].errorContainer);
            }
        }
    }
    preferences.putUInt(SHELLY_COUNT_KEY, upperLimit);
    preferences.end();
}

/* *********************************  life data*/

void eprom_clearLifeData()
{
    std::lock_guard<std::mutex> lock(s_epromMutex);

    preferences.begin(_LIFE_DATA, false);
    preferences.clear();
    preferences.putULong64(_TEMP_LIMIT_REACHED, 0.0);            // timestamp
    preferences.putULong64(_HEATING_SWITCHED_ON_LAST_TIME, 0.0); // timestamp

    preferences.end();
}

void eprom_getLifeData(LIFE_DATA &data)
{
    std::lock_guard<std::mutex> lock(s_epromMutex);

    preferences.begin(_LIFE_DATA, true); // READ-ONLY
    data.heatingLastTime = preferences.getULong64(_TEMP_LIMIT_REACHED);
    data.tempLimitReached = preferences.getULong64(_HEATING_SWITCHED_ON_LAST_TIME);
    preferences.end();
}
