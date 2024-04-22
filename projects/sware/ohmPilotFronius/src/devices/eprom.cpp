#define __EEPROM_CPP

#include <Preferences.h>

#include "utils.h"
#include "eprom.h"
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
    bool result = true;
    stammDataUpdateWatch = true;
    uint32_t ipAsInt;
    preferences.begin(CREDENTIALS, false);

    preferences.clear();
    preferences.putString(_SSID, setup.ssid);
    preferences.putString(_PASSWORD, setup.passwd);
    preferences.putUInt(_HEIZSTAB_LEISTUNG_IN_WATT, setup.heizstab_leistung_in_watt);
    preferences.putUInt(_TEMP_MAX_IN_GRAD, setup.tempMaxAllowedInGrad);
    preferences.putUInt(_TEMP_MIN_IN_GRAD, setup.tempMinInGrad);

    /* preferences.putUInt(_INVERTER_IP, setup.ipInverter);
    // DBGf("eprom_storeSetup : IP address %d", setup.ipInverter);
    result = false;
    if (strlen(setup.inverter) < 3)
        ipAsInt = 0;
    else
    {
        ipAsInt = ipv4_string_to_int(setup.inverter, &result);
        if (!result)
        {
            ipAsInt = 0;
            DBGf("ERPROM - Error in converting Inverter IPAdress %s", setup.inverter);
        }
    } */
   // preferences.putUInt(_INVERTER_IP, ipAsInt);
    preferences.putString(_INVERTER_IP, setup.inverter);
    preferences.putBool(_EXTERNER_SPEICHER, setup.externerSpeicher);
    preferences.putChar(_EXTERNER_SPEICHER_PRIORI, setup.externerSpeicherPriori);
    preferences.putUInt(_PID_TARGET_POWER, setup.pid_powerWhichNeedNotConsumed);
    preferences.putUInt(_PID_DIG_OUT_ON_DELAY_MS, setup.pid_min_time_without_contoller_inMS);

   /*  if (strlen(setup.amisReaderHost) < 3)
        ipAsInt = 0;
    else
    {
        ipAsInt = ipv4_string_to_int(setup.amisReaderHost, &result);
        if (!result)
        {
            ipAsInt = 0;
            DBGf("ERPROM - Error in converting AmisREADER IPAdress %s", setup.amisReaderHost);
        }
    }

    preferences.putUInt(_AMIS_READER_HOST, ipAsInt); */
    preferences.putString(_AMIS_READER_HOST, setup.amisReaderHost); 
    preferences.putString(_AMIS_READER_KEY, setup.amisKey);

    preferences.putString(_MQTT_HOST, setup.mqttHost);
    preferences.putString(_MQTT_PASSWD, setup.mqttPass);
    preferences.putString(_MQTT_USER, setup.mqttUser);

    preferences.putString(_INFLUX_HOST, setup.influxHost);
    preferences.putString(_INFLUX_BUCKET, setup.influxBucket);
    preferences.putString(_INFLUX_ORG, setup.influxOrg);
    preferences.putString(_INFLUX_TOKEN, setup.influxToken);

    /* else
        DBGf("EPROM - amis reader %s, ip: %d", setup.amisReaderHost.c_str(), setup.ipAmisReaderHost);
 */

    /*  preferences.putUInt(_PID_DIG_OUT_OFF_DELAY_MS, setup.pid_min_time_before_switch_off_channel_inMS);
     preferences.putUInt(_PID_MIN_ON_TIME_MS, setup.pid_min_time_for_dig_output_inMS);


     preferences.putFloat(_PID_P, setup.pid_p);
     preferences.putFloat(_PID_I, setup.pid_i);
     preferences.putFloat(_PID_D, setup.pid_d); */
    // only for testing pid controller
    preferences.putDouble(_EN_LOAD, setup.additionalLoad);
    preferences.putInt(_EN_FORCE_HEATING, setup.forceHeating);

    preferences.end();
}
/* String &eprom_getInverter(Setup &setup, String &inverter)
{

    delay(5000);
    preferences.begin(CREDENTIALS, false);
    strcpy(setup.inverter,preferences.getString(_INVERTER_IP).c_str());
   setup.inverter = preferences.getString(_INVERTER_IP);
    DBGf("getInverter: %d", setup.ipInverter);
    bool result = true;
    DBG("===>IP-Inverter eprom_getSetup: ");
    DBGf("%d", setup.ipInverter);
    DBG(" as string: ");

    ipv4_int_to_string(setup.inverter, setup.ipInverter, &result);
    // setup.inverter = res;
    if (!result)
        DBGf("eprom_getInverter - Error in converting IPAdress!!"); 
    DBGf("eprom_getInverter::   IP-Inverter as string: %s", setup.inverter);

    preferences.end();
    return inverter;
}
 */
void eprom_isInit()
{
    preferences.begin(CREDENTIALS, false);
    if (preferences.getString(_SSID, "") == NULL)
    {
        DBGf("epeprom_rom_isInit - flash hasn't been used never before ! - reinit ");
        eprom_test_write_Eprom("", "");
    }
    else
    {
        DBGf("eprom_isInit:: Nothing to do");
    }
    preferences.end();
}

void eprom_getSetup(Setup &setup)
{
    memset(&setup, 0, sizeof(Setup));
    preferences.begin(CREDENTIALS, false);
    String ssid, passwd;
    bool result = true;

    ssid = preferences.getString(_SSID, "");
    passwd = preferences.getString(_PASSWORD, "");

    if (ssid == "" || passwd == "")
    {
        DBGf("No values saved for ssid or password");
        strcpy(setup.ssid, "---");
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

    strncpy(setup.amisReaderHost,preferences.getString(_AMIS_READER_HOST).c_str(),INET_ADDRSTRLEN);
    

   /*  ipv4_int_to_string(setup.amisReaderHost, setup.ipAmisReaderHost, &result);
    if (!result)
        DBGf("ERPROM - Error in converting AmisReader IPAdress!!"); */
    DBGf("eprom_getSetup() .. AmisReaderHost:  %s",  setup.amisReaderHost);

    // String key = preferences.getString(_AMIS_READER_KEY);
    strncpy(setup.amisKey, preferences.getString(_AMIS_READER_KEY).c_str(), AMIS_KEY_LEN - 1);

strncpy(setup.inverter,preferences.getString(_INVERTER_IP).c_str(),INET_ADDRSTRLEN);

    /* 
    ipv4_int_to_string(setup.inverter, setup.ipInverter, &result);
    if (!result)
        DBGf("ERPROM - Error in converting Inverter IPAdress!!"); */

    DBGf("===>IP-Inverter eprom_getSetup: as string: %s ",  setup.inverter);

    // DBGf("EPROM::   IP-Inverter as string: %s", setup.inverter);
    setup.externerSpeicher = preferences.getBool(_EXTERNER_SPEICHER);
    setup.externerSpeicherPriori = preferences.getChar(_EXTERNER_SPEICHER_PRIORI);
    /* setup.pid_p = preferences.getFloat(_PID_P);
    setup.pid_i = preferences.getFloat(_PID_I);
    setup.pid_d = preferences.getFloat(_PID_D); */

    setup.pid_min_time_without_contoller_inMS = preferences.getUInt(_PID_DIG_OUT_ON_DELAY_MS);
    setup.pid_powerWhichNeedNotConsumed = preferences.getUInt(_PID_TARGET_POWER);
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

    setup.additionalLoad = preferences.getDouble(_EN_LOAD);
    setup.forceHeating = preferences.getInt(_EN_FORCE_HEATING);

    // DBGf("eprom_getSetup() .. AmisReaderHost: %s, Key: %s", setup.amisReaderHost, setup.amisKey);
    // DBGf("EPROM::   IP-Inverter %d as string: %s", setup.ipInverter, setup.inverter.c_str());
    preferences.end();
}

void eprom_test_write_Eprom(const char *wlanE, const char *passW)
{
    Setup setup;
    /* char bu[100];
    memset(bu, 0, 100);
 */
    strncpy(setup.ssid, wlanE, LEN_WLAN - 1);
    strncpy(setup.passwd, passW, LEN_WLAN - 1);
    DBGf("eprom_test_write_Eprom BEGIN ...WLAN: %s, Passwd: %s", wlanE, setup.passwd);

    setup.heizstab_leistung_in_watt = 5000;
    setup.tempMaxAllowedInGrad = 80;
    setup.tempMinInGrad = 10;
    strcpy(setup.inverter, "10.0.0.2");

    bool result = true;
/* 
    setup.ipInverter = ipv4_string_to_int(setup.inverter, &result);
    if (!result)
        DBGf("IP translate did not succeed.");
 */
    setup.externerSpeicher = true;
    setup.externerSpeicherPriori = '1';
    /*  setup.pid_p = 1.0;
     setup.pid_i = 0.5;
     setup.pid_d = 0.01; */

    setup.pid_min_time_without_contoller_inMS = 5000;
    /*  setup.pid_min_time_before_switch_off_channel_inMS = 2000;
     setup.pid_min_time_for_dig_output_inMS = 10000; */
    setup.pid_powerWhichNeedNotConsumed = 10;
    strcpy(setup.mqttHost, "---");
    strcpy(setup.mqttPass, "MQTT_PASS");
    strcpy(setup.mqttUser, "MQTT_USER");
    strcpy(setup.influxHost, "http://rantanplan-ethernet:8086");
    strcpy(setup.influxBucket, "energieJunkies");
    strcpy(setup.influxOrg, "d727c1fb692f26f9");
    strcpy(setup.influxToken, "Zr0fsPmRgvNr0znkbudQNZBnGDHjkBOT41X4wJwZcoMMOAFVLy5eLtIpqlffQ966oQOD4aSmrTtdDX5LcVVu5Q==");

    strcpy(setup.amisReaderHost, "10.0.0.21");
/* 
    setup.ipAmisReaderHost = ipv4_string_to_int(setup.amisReaderHost, &result);
    if (!result)
        DBGf("IP translate did not succeed."); */
    strncpy(setup.amisKey, "ABCDFGASDFGDSAERTQWEREW§EWERQEEE", AMIS_KEY_LEN - 1);

    setup.forceHeating = 10;
    setup.additionalLoad = 0.0;

    DBGf("eprom_test_write_Eprom END");

    eprom_storeSetup(setup);
}

void printEprom(Setup &setup)
{
    char buffer[600];
    memset(buffer, 0, 600);
    DBGf("\nprintEprom ============================================ ");
    sprintf(buffer, "EPROM out \n\n WLAN: %s, Passwd: %s HeizstabLeistungInWatt: %d, AusschaltTempInC: %d MindesttempInGrad: %d externer SPeicher: %d Priorität: %c Influx: %s ,inverter: %s,  Controller_OUT_ON_DELAY_MS: %d   forceHeating: %d , AdditionalLoad: %.3f, amisReader: %s, mqttServer: %s, mqttUser: %s, mqqtPwd: %s, influxHost: %s, influxOrg: %s, influxToken %s, influxBucket: %s, AmisHost: \n\nEND OF EPROM",
            setup.ssid, setup.passwd, setup.heizstab_leistung_in_watt, setup.tempMaxAllowedInGrad, setup.tempMinInGrad, setup.externerSpeicher, setup.externerSpeicherPriori, setup.influxOrg, setup.inverter, setup.pid_min_time_without_contoller_inMS, setup.forceHeating, setup.additionalLoad, setup.amisReaderHost, setup.mqttHost, setup.mqttUser, setup.mqttPass, setup.influxHost, setup.influxOrg, setup.influxToken, setup.influxBucket,setup.amisReaderHost);
    DBGf("%s", buffer);

    DBGf("\n==========================(%d)============================== ", strlen(buffer));
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