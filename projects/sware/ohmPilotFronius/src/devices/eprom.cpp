#define __EEPROM_CPP

#include <Preferences.h>

#include "utils.h"
#include "eprom.h"

#define SETUP_CREDENTIALS "setup"
#define SHELLY_EPROM "shelly"
#define SHELLY_DEVICE_NAME "sdn"
#define SHELLY_MAC "smac"
#define SHELLY_IP "sip"
#define SHELLY_PORT "spt"

// #include "tft.h"

// https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/#example2

static Preferences preferences;
static bool stammDataUpdateWatch = false;

bool eprom_stammDataUpdate()
{
    return stammDataUpdateWatch;
}
void eprom_stammDataUpdateReset()
{
    stammDataUpdateWatch = false;
}

void eprom_storeSetup(Setup &setup)
{
    LOG_DEBUG("EEPROM","Store begin");
    //preferences.clear();
    //bool result = true;
    stammDataUpdateWatch = true;
    //uint32_t ipAsInt;
    preferences.begin(SETUP_CREDENTIALS, false);
    preferences.putBytes("setup_block", &setup, sizeof(Setup));

    /*
        preferences.clear();
         preferences.putString(_SSID, setup.ssid);
        preferences.putString(_PASSWORD, setup.passwd);
        preferences.putUInt(_HEIZSTAB_LEISTUNG_IN_WATT, setup.heizstab_leistung_in_watt);
        preferences.putUInt(_TEMP_MAX_IN_GRAD, setup.tempMaxAllowedInGrad);
        preferences.putUInt(_TEMP_MIN_IN_GRAD, setup.tempMinInGrad);

        preferences.putString(_INVERTER_IP, setup.inverter);
        preferences.putChar(_AKKU, setup.akku);
        preferences.putChar(_AKKU_PRIORI, setup.akkuPriori);
        preferences.putUInt(_LEGIONELLEN_SCHWELLWERT_DELTA_TIME, setup.legionellenDelta);
        preferences.putUInt(_LEGIONELLEN_SCHWELLWERT_DELTA_TEMP, setup.legionellenMaxTemp);


         preferences.putUInt(_AMIS_READER_HOST, ipAsInt);
    preferences.putString(_AMIS_READER_HOST, setup.amisReaderHost);
    preferences.putString(_AMIS_READER_KEY, setup.amisKey);

    preferences.putString(_MQTT_HOST, setup.mqttHost);
    preferences.putString(_MQTT_PASSWD, setup.mqttPass);
    preferences.putString(_MQTT_USER, setup.mqttUser);

    preferences.putString(_INFLUX_HOST, setup.influxHost);
    preferences.putString(_INFLUX_BUCKET, setup.influxBucket);
    preferences.putString(_INFLUX_ORG, setup.influxOrg);
    preferences.putString(_INFLUX_TOKEN, setup.influxToken);

    preferences.putDouble(_EPSILON_PIN_MANAGER, setup.epsilonML_PinManager);
    preferences.putInt(_EN_FORCE_HEATING, setup.forceHeating); */

    preferences.end();
    LOG_DEBUG("EEPROM", "Store end");
}

void eprom_isInit()
{
    preferences.begin(SETUP_CREDENTIALS, false);
/* 
    if (preferences.getString(_SSID, "") == NULL)
    {
        LOG_INFO(TAG_EPPROM,"epeprom_rom_isInit - flash hasn't been used never before ! - reinit ");
        eprom_test_write_Eprom("Milchbehaelter", "47754775");
    }
    else
    {
        LOG_INFO(TAG_EPPROM,"eprom_isInit:: Nothing to do");
    } */
    preferences.end();
}

bool eprom_getSetup(Setup &setup)
{
    LOG_DEBUG(TAG_EPPROM,"eprom_getSetup");
    memset(&setup, 0, sizeof(Setup));
    preferences.begin(SETUP_CREDENTIALS, false);
    size_t len = preferences.getBytes("setup_block", &setup, sizeof(Setup));
    preferences.end();
    return (len == sizeof(Setup));

#ifdef DEBUG_DEBUG
    String ssid, passwd;
    // bool result = true;

    ssid = preferences.getString(_SSID, "");
    passwd = preferences.getString(_PASSWORD, "");

    if (ssid == "" || passwd == "")
    {
        LOG_INFO(TAG_EPPROM,"eprom::eprom_getSetup No values saved for ssid or password");
        strcpy(setup.ssid, EMPTY_VALUE_IN_SETUP);
        strcpy(setup.passwd, "");
    }
    elseString ssid, passwd;
    // bool result = true;

    ssid = preferences.getString(_SSID, "");
    passwd = preferences.getString(_PASSWORD, "");

    if (ssid == "" || passwd == "")
    {
        LOG_INFO(TAG_EPPROM, "eprom::eprom_getSetup No values saved for ssid or password");
        strcpy(setup.ssid, EMPTY_VALUE_IN_SETUP);
        strcpy(setup.passwd, "");
    }
    else
    {
        strncpy(setup.ssid, (const char *)ssid.c_str(), LEN_WLAN - 1);
        strncpy(setup.passwd, passwd.c_str(), LEN_WLAN - 1);
    }
    //  DBGf("eprom_getSetup() .. WLAN: %s, Passwd: %s", setup.ssid, setup.passwd);
    setup.heizstab_leistung_in_watt = preferences.getUInt(_HEIZSTAB_LEISTUNG_IN_WATT);
    setup.phasen_leistung_in_watt = (unsigned int)setup.heizstab_leistung_in_watt / 3; // pre calculation
    setup.tempMaxAllowedInGrad = preferences.getUInt(_TEMP_MAX_IN_GRAD);
    setup.tempMinInGrad = preferences.getUInt(_TEMP_MIN_IN_GRAD);

    strncpy(setup.amisReaderHost, preferences.getString(_AMIS_READER_HOST).c_str(), INET_ADDRSTRLEN);

    /*  ipv4_int_to_string(setup.amisReaderHost, setup.ipAmisReaderHost, &result);
     if (!result)
         DBGf("ERPROM - Error in converting AmisReader IPAdress!!"); */
    // DBGf("eprom_getSetup() .. AmisReaderHost:  %s", setup.amisReaderHost);

    // String key = preferences.getString(_AMIS_READER_KEY);
    strncpy(setup.amisKey, preferences.getString(_AMIS_READER_KEY).c_str(), AMIS_KEY_LEN - 1);

    strncpy(setup.inverter, preferences.getString(_INVERTER_IP).c_str(), INET_ADDRSTRLEN);

    /*
    ipv4_int_to_string(setup.inverter, setup.ipInverter, &result);
    if (!result)
        DBGf("ERPROM - Error in converting Inverter IPAdress!!"); */

    // DBGf("===>IP-Inverter eprom_getSetup: as string: %s ", setup.inverter);

    // DBGf("EPROM::   IP-Inverter as string: %s", setup.inverter);
    setup.akku = preferences.getChar(_AKKU);
    setup.akkuPriori = preferences.getChar(_AKKU_PRIORI);
    /* setup.pid_p = preferences.getFloat(_PID_P);
    setup.pid_i = pL', '100', 'Strom, references.getFloat(_PID_I);
    setup.pid_d = preferences.getFloat(_PID_D); */

    setup.legionellenDelta = preferences.getUInt(_LEGIONELLEN_SCHWELLWERT_DELTA_TIME);
    setup.legionellenMaxTemp = preferences.getUInt(_LEGIONELLEN_SCHWELLWERT_DELTA_TEMP);
    /*   setup.pid_min_time_before_switch_off_channel_inMS = preferences.getUInt(_PID_DIG_OUT_OFF_DELAY_MS);
      setup.pid_min_time_for_dig_output_inMS = preferences.getUInt(_PID_MIN_ON_TIME_MS);
      setup.pid_powerWhichNeedNotConsumed = preferences.getUInt(_PID_TARGET_POWER);
      setup.pidChanged = false; */

    strncpy(setup.mqttHost, preferences.getString(_MQTT_HOST).c_str(), MQTT_HOST_LEN - 1);
    strncpy(setup.mqttPass, preferences.getString(_MQTT_PASSWD).c_str(), MQTT_PASS_LEN - 1);
    strncpy(setup.mqttUser, preferences.getString(_MQTT_USER).c_str(), MQTT_USER_LEN - 1);

    strncpy(setup.influxHost, preferences.getString(_INFLUX_HOST).c_str(), INFLUX_HOST_LEN - 1);
    strncpy(setup.influxBucket, preferences.getString(_INFLUX_BUCKET).c_str(), INFLUX_BUCKET_LEN - 1);
    strncpy(setup.influxOrg, preferences.getString(_INFLUX_ORG).c_str(), INFLUX_ORG_LEN - 1);
    strncpy(setup.influxToken, preferences.getString(_INFLUX_TOKEN).c_str(), INFLUX_TOKEN_LEN - 1);

    setup.epsilonML_PinManager = preferences.getDouble(_EPSILON_PIN_MANAGER);
    setup.forceHeating = preferences.getInt(_EN_FORCE_HEATING);

    // DBGf("eprom_getSetup() .. AmisReaderHost: %s, Key: %s", setup.amisReaderHost, setup.amisKey);
    // DBGf("EPROM::   IP-Inverter %d as string: %s", setup.ipInverter, setup.inverter.c_str());

    {
        strncpy(setup.ssid, (const char *)ssid.c_str(), LEN_WLAN - 1);
        strncpy(setup.passwd, passwd.c_str(), LEN_WLAN - 1);
    }
    //  DBGf("eprom_getSetup() .. WLAN: %s, Passwd: %s", setup.ssid, setup.passwd);
    setup.heizstab_leistung_in_watt = preferences.getUInt(_HEIZSTAB_LEISTUNG_IN_WATT);
    setup.phasen_leistung_in_watt = (unsigned int)setup.heizstab_leistung_in_watt / 3; // pre calculation
    setup.tempMaxAllowedInGrad = preferences.getUInt(_TEMP_MAX_IN_GRAD);
    setup.tempMinInGrad = preferences.getUInt(_TEMP_MIN_IN_GRAD);

    strncpy(setup.amisReaderHost, preferences.getString(_AMIS_READER_HOST).c_str(), INET_ADDRSTRLEN);

    /*  ipv4_int_to_string(setup.amisReaderHost, setup.ipAmisReaderHost, &result);
     if (!result)
         DBGf("ERPROM - Error in converting AmisReader IPAdress!!"); */
    // DBGf("eprom_getSetup() .. AmisReaderHost:  %s", setup.amisReaderHost);

    // String key = preferences.getString(_AMIS_READER_KEY);
    strncpy(setup.amisKey, preferences.getString(_AMIS_READER_KEY).c_str(), AMIS_KEY_LEN - 1);

    strncpy(setup.inverter, preferences.getString(_INVERTER_IP).c_str(), INET_ADDRSTRLEN);

    /*
    ipv4_int_to_string(setup.inverter, setup.ipInverter, &result);
    if (!result)
        DBGf("ERPROM - Error in converting Inverter IPAdress!!"); */

    // DBGf("===>IP-Inverter eprom_getSetup: as string: %s ", setup.inverter);

    // DBGf("EPROM::   IP-Inverter as string: %s", setup.inverter);
    setup.akku = preferences.getChar(_AKKU);
    setup.akkuPriori = preferences.getChar(_AKKU_PRIORI);
    /* setup.pid_p = preferences.getFloat(_PID_P);
    setup.pid_i = pL', '100', 'Strom, references.getFloat(_PID_I);
    setup.pid_d = preferences.getFloat(_PID_D); */

    setup.legionellenDelta = preferences.getUInt(_LEGIONELLEN_SCHWELLWERT_DELTA_TIME);
    setup.legionellenMaxTemp = preferences.getUInt(_LEGIONELLEN_SCHWELLWERT_DELTA_TEMP);
    /*   setup.pid_min_time_before_switch_off_channel_inMS = preferences.getUInt(_PID_DIG_OUT_OFF_DELAY_MS);
      setup.pid_min_time_for_dig_output_inMS = preferences.getUInt(_PID_MIN_ON_TIME_MS);
      setup.pid_powerWhichNeedNotConsumed = preferences.getUInt(_PID_TARGET_POWER);
      setup.pidChanged = false; */

    strncpy(setup.mqttHost, preferences.getString(_MQTT_HOST).c_str(), MQTT_HOST_LEN - 1);
    strncpy(setup.mqttPass, preferences.getString(_MQTT_PASSWD).c_str(), MQTT_PASS_LEN - 1);
    strncpy(setup.mqttUser, preferences.getString(_MQTT_USER).c_str(), MQTT_USER_LEN - 1);

    strncpy(setup.influxHost, preferences.getString(_INFLUX_HOST).c_str(), INFLUX_HOST_LEN - 1);
    strncpy(setup.influxBucket, preferences.getString(_INFLUX_BUCKET).c_str(), INFLUX_BUCKET_LEN - 1);
    strncpy(setup.influxOrg, preferences.getString(_INFLUX_ORG).c_str(), INFLUX_ORG_LEN - 1);
    strncpy(setup.influxToken, preferences.getString(_INFLUX_TOKEN).c_str(), INFLUX_TOKEN_LEN - 1);

    setup.epsilonML_PinManager = preferences.getDouble(_EPSILON_PIN_MANAGER);
    setup.forceHeating = preferences.getInt(_EN_FORCE_HEATING);

    // DBGf("eprom_getSetup() .. AmisReaderHost: %s, Key: %s", setup.amisReaderHost, setup.amisKey);
    // DBGf("EPROM::   IP-Inverter %d as string: %s", setup.ipInverter, setup.inverter.c_str());
    #endif
  
}

bool eprom_test_write_Eprom(const char *wlanE, const char *passW)
{
    Setup setup;
    /* char bu[100];
    memset(bu, 0, 100);
 */
    strncpy(setup.ssid, wlanE, LEN_WLAN - 1);
    strncpy(setup.passwd, passW, LEN_WLAN - 1);
    LOG_DEBUG(TAG_EPPROM,"eprom_test_write_Eprom BEGIN ...WLAN: %s, Passwd: %s", wlanE, setup.passwd);

    setup.heizstab_leistung_in_watt = 4500;
    setup.tempMaxAllowedInGrad = 80;
    setup.tempMinInGrad = 5;
    strcpy(setup.inverter, "10.0.0.22");

    setup.akku = 0;
    setup.akkuPriori = 0;
    /*  setup.pid_p = 1.0;
     setup.pid_i = 0.5;
     setup.pid_d = 0.01; */

    setup.legionellenDelta = 7UL * 24 * 3600 * 1000;
    /*  setup.pid_min_time_before_switch_off_channel_inMS = 2000;
     setup.pid_min_time_for_dig_output_inMS = 10000; */
    setup.legionellenMaxTemp = 70;
    strcpy(setup.mqttHost, EMPTY_VALUE_IN_SETUP);
    strcpy(setup.mqttPass, "MQTT_PASS");
    strcpy(setup.mqttUser, "MQTT_USER");
    strcpy(setup.influxHost, "http://rantanplan-ethernet:8086");
    //strcpy(setup.influxHost, EMPTY_VALUE_IN_SETUP);
    strcpy(setup.influxBucket, "energieJunkies");
    strcpy(setup.influxOrg, "d727c1fb692f26f9");
    strcpy(setup.influxToken, "Zr0fsPmRgvNr0znkbudQNZBnGDHjkBOT41X4wJwZcoMMOAFVLy5eLtIpqlffQ966oQOD4aSmrTtdDX5LcVVu5Q=="); 
    //strcpy(setup.influxToken, "---");
    strcpy(setup.amisReaderHost, "192.168.178.45");
    setup.epsilonML_PinManager=0.05;
    
    strncpy(setup.amisKey, "9865888B5CC739E9F575053E7868BC34", AMIS_KEY_LEN - 1);
    setup.wattSetupForTest = 0;

    setup.forceHeating = 0;

    LOG_DEBUG(TAG_EPPROM,"eprom::eprom_test_write_Eprom END");

    eprom_storeSetup(setup);
    return true;
}

void printEprom(Setup &setup)
{
    char buffer[2000];
    memset(buffer, 0, 2000);
    LOG_DEBUG(TAG_EPPROM,"eprom::printEprom ============================================ ");
    sprintf(buffer, "EPROM out \n\n WLAN: %s, Passwd: %s HeizstabLeistungInWatt: %d, AusschaltTempInC: %d MindesttempInGrad: %d externer SPeicher: %d Priorität: %d Influx: %s ,inverter: %s,  LegionellenCheckDelta: %d   forceHeating: %d , Epsilon-PinManager: %.3f, amisReader: %s, mqttServer: %s, mqttUser: %s, mqqtPwd: %s, influxHost: %s, influxOrg: %s, influxToken %s, influxBucket: %s,  \n\nEND OF EPROM",
            setup.ssid, setup.passwd, setup.heizstab_leistung_in_watt, setup.tempMaxAllowedInGrad, setup.tempMinInGrad, setup.akku, setup.akkuPriori, setup.influxOrg, setup.inverter, setup.legionellenDelta, setup.forceHeating, setup.epsilonML_PinManager, setup.amisReaderHost, setup.mqttHost, setup.mqttUser, setup.mqttPass, setup.influxHost, setup.influxOrg, setup.influxToken, setup.influxBucket);
    LOG_DEBUG(TAG_EPPROM,"%s", buffer);

    // DBGf("\n==========================(%d)============================== ", strlen(buffer));
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
/* *************************** SHELLY */

#define SHELLY_DEVICE_NAME "sdn"
#define SHELLY_MAC "smac"
#define SHELLY_IP "sip"
#define SHELLY_PORT "spt"

void eprom_store_shelly(ALL_SHELLY_DEVICES *allDevices, unsigned upperLimit)
{
    LOG_INFO(TAG_EPPROM,"eprom::eprom_store_shelly\n");
    preferences.begin(SHELLY_EPROM, false);
    preferences.clear();
    for (int i = 0; i < upperLimit; i++)
    {
        if (allDevices[i].valid == true)
        {
            if (allDevices[i].errorContainer == NULL)
            {
                LOG_INFO(TAG_EPPROM,"eprom:: index: %d, device name: %s, mac: %s, ip: %s, port: %d\n", i, allDevices[i].shellyDevice->name, allDevices[i].shellyDevice->mac, allDevices[i].shellyDevice->ip, allDevices[i].shellyDevice->port);
                preferences.putString(SHELLY_DEVICE_NAME, allDevices[i].shellyDevice->name);
                preferences.putString(SHELLY_MAC, allDevices[i].shellyDevice->mac);
                preferences.putString(SHELLY_IP, allDevices[i].shellyDevice->ip);
                preferences.putUInt(SHELLY_PORT, allDevices[i].shellyDevice->port);
                free(allDevices[i].shellyDevice);
            }
            else
            {
                LOG_ERROR(TAG_EPPROM,"eprom::eprom_store_shelly() - ERROR Msg: %s, Method: %s\n", allDevices[i].errorContainer->errorMessage, allDevices[i].errorContainer->usedMethod);
                free(allDevices[i].errorContainer);
            }
        }
    }
    preferences.end();
}

/* *********************************  life data*/

void eprom_clearLifeData()
{
    preferences.begin(_LIFE_DATA, false);
    preferences.clear();
    preferences.putULong64(_TEMP_LIMIT_REACHED, 0.0);            // timestamp
    preferences.putULong64(_HEATING_SWITCHED_ON_LAST_TIME, 0.0); // timestamp

    preferences.end();
}

void eprom_getLifeData(LIFE_DATA &data)
{
    preferences.begin(_LIFE_DATA, false);
    data.heatingLastTime = preferences.getULong64(_TEMP_LIMIT_REACHED);
    data.tempLimitReached = preferences.getULong64(_HEATING_SWITCHED_ON_LAST_TIME);
    preferences.end();
}