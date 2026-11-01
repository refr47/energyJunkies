#define __EEPROM_CPP

#include <Preferences.h>
#include <atomic>
#include <mutex>

#include "utils.h"
#include "eprom.h"
#include "setupValidation.h"

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

bool eprom_storeSetup(Setup &setup)
{
    JsonDocument validation;
    if (!validateSetup(setup, validation.to<JsonArray>()) || validation.overflowed())
        return false;
    std::lock_guard<std::mutex> lock(s_epromMutex);
    if (!preferences.begin(SETUP_CREDENTIALS, false))
        return false;

    bool ok = preferences.putBytes("setup_block", &setup, sizeof(Setup)) == sizeof(Setup);
    if (ok)
        ok = preferences.putUInt(NVS_BLOCK_MAGIC_KEY, NVS_BLOCK_MAGIC_VAL) == sizeof(uint32_t);

    if (ok) ok = preferences.putUInt("schema", 1) == sizeof(uint32_t);

    Setup verifyBuf;
    if (ok)
    {
        const size_t len = preferences.getBytes("setup_block", &verifyBuf, sizeof(Setup));
        ok = len == sizeof(Setup)
            && preferences.getUInt("schema", 0) == 1
            && preferences.getUInt(NVS_BLOCK_MAGIC_KEY, 0) == NVS_BLOCK_MAGIC_VAL
            && memcmp(&setup, &verifyBuf, sizeof(Setup)) == 0;
    }
    preferences.end();
    if (ok)
        s_stammDataUpdateWatch.store(true);
    else
        LOG_ERROR(TAG_EPPROM, "eprom_storeSetup: NVS write/verify failed");
    return ok;
}

void eprom_isInit()
{
    std::lock_guard<std::mutex> lock(s_epromMutex);
    if (preferences.begin(SETUP_CREDENTIALS, true)) preferences.end();
}

bool eprom_getSetup(Setup &setup)
{
    setupDefaults(setup);
    Setup candidate{};
    std::lock_guard<std::mutex> lock(s_epromMutex);
    if (!preferences.begin(SETUP_CREDENTIALS, true)) return false;
    const unsigned schema = preferences.getUInt("schema", 0); // 0: legacy raw Setup
    const bool headerOK = schema <= 1 && preferences.getUInt(NVS_BLOCK_MAGIC_KEY, 0) == NVS_BLOCK_MAGIC_VAL
        && preferences.getBytesLength("setup_block") == sizeof(Setup);
    const size_t len = headerOK ? preferences.getBytes("setup_block", &candidate, sizeof(candidate)) : 0;
    preferences.end();
    if (len != sizeof(candidate)) return false;
    // bool object representations must be checked as bytes before using them.
    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(&candidate);
    if (bytes[offsetof(Setup, akku)] > 1 || bytes[offsetof(Setup, setupChanged)] > 1) return false;
    JsonDocument validation;
    if (!validateSetup(candidate, validation.to<JsonArray>()) || validation.overflowed()) return false;
    candidate.setupChanged = false;
    setup = candidate;
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
