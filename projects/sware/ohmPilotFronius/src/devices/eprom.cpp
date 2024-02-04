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
    stammDataUpdateWatch = true;
    preferences.begin(CREDENTIALS, false);

    preferences.clear();
    preferences.putString(_SSID, setup.ssid);
    preferences.putString(_PASSWORD, setup.passwd);
    preferences.putUInt(_HEIZSTAB_LEISTUNG_IN_WATT, setup.heizstab_leistung_in_watt);
    preferences.putUInt(_TEMP_MAX_IN_GRAD, setup.tempMaxAllowedInGrad);
    preferences.putUInt(_TEMP_MIN_IN_GRAD, setup.tempMinInGrad);
    preferences.putUInt(_INVERTER_IP, setup.ipInverter);
    preferences.putBool(_EXTERNER_SPEICHER, setup.externerSpeicher);
    preferences.putChar(_EXTERNER_SPEICHER_PRIORI, setup.externerSpeicherPriori);
    preferences.putUInt(_PID_DIG_OUT_ON_DELAY_MS, setup.pid_min_time_without_contoller_inMS);
    preferences.putUInt(_PID_DIG_OUT_OFF_DELAY_MS, setup.pid_min_time_before_switch_off_channel_inMS);
    preferences.putUInt(_PID_MIN_ON_TIME_MS, setup.pid_min_time_for_dig_output_inMS);
    preferences.putUInt(_PID_TARGET_POWER, setup.pid_powerWhichNeedNotConsumed);

    preferences.putFloat(_PID_P, setup.pid_p);
    preferences.putFloat(_PID_I, setup.pid_i);
    preferences.putFloat(_PID_D, setup.pid_d);
    // only for testing pid controller
    preferences.putDouble(_EN_LOAD, setup.additionalLoad);
    preferences.putInt(_EN_BIAS, setup.exportWatt);
    bool result=true;
    setup.ipAmisReaderHost = ipv4_string_to_int(setup.amisReaderHost, &result);
    if (!result)
        DBGf("ERPROM - Error in converting AmisReader IPAdress!!");

    preferences.putInt(_AMIS_READER_HOST, setup.ipAmisReaderHost);
    preferences.putString(_AMIS_READER_KEY, setup.amisKey);
    preferences.end();
}

void eprom_isInit()
{
    preferences.begin(CREDENTIALS, false);
    if (preferences.getString(_SSID, "") == NULL)
    {
        DBGf("eprom_isInit - flash hasn't been used never before ! - reinit ");
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
    preferences.begin(CREDENTIALS, false);
    String ssid, passwd;

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
    setup.heizstab_leistung_in_watt = preferences.getUInt(_HEIZSTAB_LEISTUNG_IN_WATT);
    setup.phasen_leistung_in_watt = (unsigned int)setup.heizstab_leistung_in_watt / 3; // pre calculation
    setup.tempMaxAllowedInGrad = preferences.getUInt(_TEMP_MAX_IN_GRAD);
    setup.tempMinInGrad = preferences.getUInt(_TEMP_MIN_IN_GRAD);

    setup.ipInverter = preferences.getUInt(_INVERTER_IP);
    bool result = true;
    /*   DBG("===>IP-Inverter eprom_getSetup: ");
      DBGln(setup.ipInverter);
      DBG(" as string: "); */
    setup.ipInverterAsString = ipv4_int_to_string(setup.ipInverter, &result);
    if (!result)
        DBGf("ERPROM - Error in converting IPAdress!!");

    setup.externerSpeicher = preferences.getBool(_EXTERNER_SPEICHER);
    setup.externerSpeicherPriori = preferences.getChar(_EXTERNER_SPEICHER_PRIORI);
    setup.pid_p = preferences.getFloat(_PID_P);
    setup.pid_i = preferences.getFloat(_PID_I);
    setup.pid_d = preferences.getFloat(_PID_D);

    setup.pid_min_time_without_contoller_inMS = preferences.getUInt(_PID_DIG_OUT_ON_DELAY_MS);
    setup.pid_min_time_before_switch_off_channel_inMS = preferences.getUInt(_PID_DIG_OUT_OFF_DELAY_MS);
    setup.pid_min_time_for_dig_output_inMS = preferences.getUInt(_PID_MIN_ON_TIME_MS);
    setup.pid_powerWhichNeedNotConsumed = preferences.getUInt(_PID_TARGET_POWER);
    setup.pidChanged = false;
    setup.additionalLoad = preferences.getDouble(_EN_LOAD);
    setup.exportWatt = preferences.getInt(_EN_BIAS);
    setup.ipAmisReaderHost = preferences.getUInt(_AMIS_READER_HOST);
    setup.amisReaderHost = ipv4_int_to_string(setup.ipAmisReaderHost, &result);
    if (!result)
        DBGf("ERPROM - Error in converting AmisReader IPAdress!!");

    String key= preferences.getString(_AMIS_READER_KEY);
    strncpy(setup.amisKey,key.c_str(), AMIS_KEY_LEN - 1);
    DBGf("eprom_getSetup() .. AmisReaderHost: %s, Key: %s",setup.amisReaderHost, setup.amisKey);
    preferences.end();
}

void eprom_test_write_Eprom(const char *wlanE, const char *passW)
{
    Setup setup;

    strncpy(setup.ssid, wlanE, LEN_WLAN - 1);
    strncpy(setup.passwd, passW, LEN_WLAN - 1);
    DBGf("eprom_test_write_Eprom BEGIN ...WLAN: %s, Passwd: %s", wlanE, setup.passwd);

    setup.heizstab_leistung_in_watt = 5000;
    setup.tempMaxAllowedInGrad = 80;
    setup.tempMinInGrad = 10;
    String ipInv = "10.0.0.2";
    bool result = true;

    setup.ipInverter = ipv4_string_to_int(ipInv, &result);
    if (!result)
        DBGf("IP translate did not succeed.");

    setup.externerSpeicher = false;
    setup.externerSpeicherPriori = '1';
    setup.pid_p = 1.0;
    setup.pid_i = 0.5;
    setup.pid_d = 0.01;

    setup.pid_min_time_without_contoller_inMS = 5000;
    setup.pid_min_time_before_switch_off_channel_inMS = 2000;
    setup.pid_min_time_for_dig_output_inMS = 10000;
    setup.pid_powerWhichNeedNotConsumed = 10;
    setup.exportWatt = 10;
    setup.additionalLoad = 0.0;
    String ipAmis = "10.0.0.21";
    setup.ipAmisReaderHost = ipv4_string_to_int(ipAmis, &result);
    if (!result)
        DBGf("IP translate did not succeed.");
    strncpy(setup.amisKey, "ABCDFGASDFGDSAERTQWEREW§EWERQEEE", AMIS_KEY_LEN - 1);
    DBGf("eprom_test_write_Eprom END");

    eprom_storeSetup(setup);
}

static void printEprom(Setup &setup)
{
    char buffer[500];
    sprintf(buffer, "EPROM out \n\n WLAN: %s, Passwd: %s HeizstabLeistungInWatt: %d, AusschaltTempInC: %d MindesttempInGrad: %d externer SPeicher: %d Priorität: %c TCP: %d PID_P: %f  PID_I: %f PID_D %f  DIG_OUT_ON_DELAY_MS: %d DIG_OUT_OFF_DELAY_MS %d MIN_ON_TIME_MS %d TARGET_POWER %d pidChanged: %d, ExportWatt: %d , AdditionalLoad: %.3f, amisReader: %s----- \n\nEND OF EPROM",
            setup.ssid, setup.passwd, setup.heizstab_leistung_in_watt, setup.tempMaxAllowedInGrad, setup.tempMinInGrad, setup.externerSpeicher, setup.externerSpeicherPriori, setup.ipInverter, setup.pid_p, setup.pid_i, setup.pid_d, setup.pid_min_time_without_contoller_inMS, setup.pid_min_time_before_switch_off_channel_inMS, setup.pid_min_time_for_dig_output_inMS, setup.pid_powerWhichNeedNotConsumed, setup.pidChanged, setup.exportWatt, setup.additionalLoad, setup.amisReaderHost);
    DBGf("%s", buffer);
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