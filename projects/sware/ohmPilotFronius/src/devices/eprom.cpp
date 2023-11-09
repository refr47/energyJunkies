#define __EEPROM_CPP

#include <Preferences.h>

#include "utils.h"
#include "eprom.h"
#include "tft.h"

// https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/#example2

static Preferences preferences;

void eprom_storeSetup(Setup &setup)
{
    preferences.begin(CREDENTIALS, false);
    preferences.clear();
    preferences.putString(_SSID, setup.ssid);
    preferences.putString(_PASSWORD, setup.passwd);
    preferences.putUInt(_HYSTERESE, setup.regelbereichHysterese);
    preferences.putUInt(_AUSSCHALT_TEMP, setup.ausschaltTempInGradCel);
    preferences.putUInt(_INVERTER_IP, setup.ipInverter);
    preferences.putBool(_EXTERNER_SPEICHER, setup.externerSpeicher);
    preferences.putChar(_EXTERNER_SPEICHER_PRIORI, setup.externerSpeicherPriori);
    preferences.putFloat(_PID_P, setup.pid_p);
    preferences.putFloat(_PID_I, setup.pid_i);
    preferences.putFloat(_PID_D, setup.pid_d);
    preferences.putUInt(_PID_DIG_OUT_ON_DELAY_MS, setup.pid_min_time_without_contoller_inMS);
    preferences.putUInt(_PID_DIG_OUT_OFF_DELAY_MS, setup.pid_min_time_before_switch_off_channel_inMS);
    preferences.putUInt(_PID_MIN_ON_TIME_MS, setup.pid_min_time_for_dig_output_inMS);
    preferences.putUInt(_PID_TARGET_POWER, setup.pid_targetPowerInWatt);

    // only for testing pid controller
    preferences.putChar(_PID_TEST, setup.testPid);
    preferences.putUInt(_EN_EXPORT, setup.exportWatt);
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
    setup.regelbereichHysterese = preferences.getUInt(_HYSTERESE);
    setup.ausschaltTempInGradCel = preferences.getUInt(_AUSSCHALT_TEMP);
    setup.ipInverter = preferences.getUInt(_INVERTER_IP);
    bool result = true;
    /*   DBG("===>IP-Inverter eprom_getSetup: ");
      DBGln(setup.ipInverter);
      DBG(" as string: "); */
    setup.ipInverterAsString = ipv4_int_to_string(setup.ipInverter, &result);
    if (!result)
        DBGf("ERPROM - Error in converting IPAdress!!");
    else
        DBGf("IP as String: %s", setup.ipInverterAsString);

    setup.externerSpeicher = preferences.getBool(_EXTERNER_SPEICHER);
    setup.externerSpeicherPriori = preferences.getChar(_EXTERNER_SPEICHER_PRIORI);
    setup.pid_p = preferences.getFloat(_PID_P);
    setup.pid_i = preferences.getFloat(_PID_I);
    setup.pid_d = preferences.getFloat(_PID_D);

    setup.pid_min_time_without_contoller_inMS = preferences.getUInt(_PID_DIG_OUT_ON_DELAY_MS);
    setup.pid_min_time_before_switch_off_channel_inMS = preferences.getUInt(_PID_DIG_OUT_OFF_DELAY_MS);
    setup.pid_min_time_for_dig_output_inMS = preferences.getUInt(_PID_MIN_ON_TIME_MS);
    setup.pid_targetPowerInWatt = preferences.getUInt(_PID_TARGET_POWER);
    setup.pidChanged = false;
    setup.testPid = preferences.getChar(_PID_TEST);
    setup.exportWatt = preferences.getUInt(_EN_EXPORT);
    preferences.end();
}

void eprom_test_write_Eprom(const char *wlanE, const char *passW)
{
    Setup setup;

    strncpy(setup.ssid, wlanE, LEN_WLAN - 1);
    strncpy(setup.passwd, passW, LEN_WLAN - 1);
    DBGf("in test writing wlan ...WLAN: %s, Passwd: %s", wlanE, setup.passwd);

    setup.regelbereichHysterese = 100;
    setup.ausschaltTempInGradCel = 80;
    String ipInv = "10.0.0.7";
    bool result = true;

    setup.ipInverter = ipv4_string_to_int(ipInv, &result);
    if (!result)
        DBGf("IP translate did not succeed.");
    DBGf("SETUP WRITE IP in uint: %d", setup.ipInverter);

    setup.externerSpeicher = false;
    setup.externerSpeicherPriori = 1;
    setup.pid_p = 1.0;
    setup.pid_i = 0.5;
    setup.pid_d = 0.0;

    setup.pid_min_time_without_contoller_inMS = 5000;
    setup.pid_min_time_before_switch_off_channel_inMS = 2000;
    setup.pid_min_time_for_dig_output_inMS = 10000;
    setup.pid_targetPowerInWatt = 10;

    eprom_storeSetup(setup);
}

static void printEprom(Setup &setup)
{
    char buffer[500];
    sprintf(buffer, "EPROM out \n WLAN: %s, Passwd: %s Hysterese: %d, AusschaltTempInC: %d externer SPeicher: %d Priorität: %c TCP: %d PID_P: %f.2 PID_I: %f.2 PID_D %f.2  DIG_OUT_ON_DELAY_MS: %d DIG_OUT_OFF_DELAY_MS %d MIN_ON_TIME_MS %d TARGET_POWER %d pidChanged: %d  ----- END OF EPROM",
            setup.ssid, setup.passwd, setup.regelbereichHysterese, setup.ausschaltTempInGradCel, setup.externerSpeicher, setup.externerSpeicherPriori, setup.ipInverter, setup.pid_p, setup.pid_i, setup.pid_i, setup.pid_min_time_without_contoller_inMS, setup.pid_min_time_before_switch_off_channel_inMS, setup.pid_min_time_for_dig_output_inMS, setup.pid_targetPowerInWatt, setup.pidChanged

    );
    DBGf("%s",buffer);
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