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
    preferences.putUInt(_HEIZPATRONE, setup.leistungHeizpatroneInW);
    preferences.putUInt(_HYSTERESE, setup.regelbereichHysterese);
    preferences.putUInt(_EINSPEISEBESCHRAENKUNG, setup.einspeiseBeschraenkingInW);
    preferences.putUInt(_MINDESTLAUFZEIT, setup.mindestLaufzeitInMin);
    preferences.putUInt(_AUSSCHALT_TEMP, setup.ausschaltTempInGradCel);
    preferences.putUInt(_INVERTER_IP, setup.ipInverter);
    preferences.putBool(_EXTERNER_SPEICHER, setup.externerSpeicher);
    preferences.putFloat(_PID_P, setup.pid_p);
    preferences.putFloat(_PID_I, setup.pid_i);
    preferences.putFloat(_PID_D, setup.pid_d);
    preferences.putUInt(_PID_DIG_OUT_ON_DELAY_MS, setup.pid_min_time_without_contoller_inMS);
    preferences.putUInt(_PID_DIG_OUT_OFF_DELAY_MS, setup.pid_min_time_before_switch_off_channel_inMS);
    preferences.putUInt(_PID_MIN_ON_TIME_MS, setup.pid_min_time_for_dig_output_inMS);
    preferences.putUInt(_PID_TARGET_POWER, setup.pid_targetPowerInWatt);

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
        DBGln("No values saved for ssid or password");
        strcpy(setup.ssid, "---");
        strcpy(setup.passwd, "");
    }
    else
    {
        strncpy(setup.ssid, (const char *)ssid.c_str(), LEN_WLAN - 1);
        strncpy(setup.passwd, passwd.c_str(), LEN_WLAN - 1);
    }
    setup.leistungHeizpatroneInW = preferences.getUInt(_HEIZPATRONE);
    setup.regelbereichHysterese = preferences.getUInt(_HYSTERESE);
    setup.ipInverter = preferences.getUInt(_INVERTER_IP);
    setup.einspeiseBeschraenkingInW = preferences.getUInt(_EINSPEISEBESCHRAENKUNG);
    setup.mindestLaufzeitInMin = preferences.getUInt(_MINDESTLAUFZEIT);
    setup.ausschaltTempInGradCel = preferences.getUInt(_AUSSCHALT_TEMP);
    setup.externerSpeicher = preferences.getBool(_EXTERNER_SPEICHER);
    bool result = true;
    // DBG("IP-Inverter: ");
    setup.ipInverterAsString = ipv4_int_to_string(setup.ipInverter, &result);
    // DBGln(setup.ipInverter);

    setup.pid_p = preferences.getFloat(_PID_P);
    setup.pid_i = preferences.getFloat(_PID_I);
    setup.pid_d = preferences.getFloat(_PID_D);

    setup.pid_min_time_without_contoller_inMS = preferences.getUInt(_PID_DIG_OUT_ON_DELAY_MS);
    setup.pid_min_time_before_switch_off_channel_inMS = preferences.getUInt(_PID_DIG_OUT_OFF_DELAY_MS);
    setup.pid_min_time_for_dig_output_inMS = preferences.getUInt(_PID_MIN_ON_TIME_MS);
    setup.pid_targetPowerInWatt = preferences.getUInt(_PID_TARGET_POWER);
    setup.pidChanged = false;
    preferences.end();
}

void eprom_test_write_Eprom(const char *wlanE, const char *passW)
{
    Setup setup;

    strncpy(setup.ssid, wlanE, LEN_WLAN - 1);
    strncpy(setup.passwd, passW, LEN_WLAN - 1);
    DBGln("in test writing wlan ...");
    DBG("WLAN: ");
    DBGln(wlanE);
    DBGln(setup.ssid);
    DBG("passw: ");
    DBGln(setup.passwd);
    setup.leistungHeizpatroneInW = 5000;
    setup.regelbereichHysterese = 100;
    setup.einspeiseBeschraenkingInW = 90;
    setup.mindestLaufzeitInMin = 2;
    setup.ausschaltTempInGradCel = 80;
    setup.externerSpeicher = false;
    String ipInv = "10.0.0.7";
    bool result = true;

    setup.ipInverter = ipv4_string_to_int(ipInv, &result);
    if (!result)
        DBGln("IP translate did not succeed.");
    DBG("IP in uint: ");
    DBGln(setup.ipInverter);
    setup.pid_p = 1.0;
    setup.pid_i = 0.5;
    setup.pid_d = 0.0;

    setup.pid_min_time_without_contoller_inMS = 5000;
    setup.pid_min_time_before_switch_off_channel_inMS = 2000;
    setup.pid_min_time_for_dig_output_inMS = 10000;
    setup.pid_targetPowerInWatt = 10;

    eprom_storeSetup(setup);
}

void eprom_test_read_Eprom()
{
    Setup setup;
    eprom_getSetup(setup);
    char buffer[500];
    sprintf(buffer, "EPROM out \n WLAN: %s, Passwd: %s HeizpatroneInWatt: %d Hysterese: %d, Einspeisebeschränkung: %d MindestLaufZeitInMin: %d AusschaltTempInC %d externer SPeicher: %d TCP: %d PID_P: %f.2 PID_I: %f.2 PID_D %f.2  DIG_OUT_ON_DELAY_MS: %d DIG_OUT_OFF_DELAY_MS %d MIN_ON_TIME_MS %d TARGET_POWER %d pidChanged: %d  ----- END OF EPROM",
            setup.ssid, setup.passwd, setup.leistungHeizpatroneInW, setup.regelbereichHysterese, setup.einspeiseBeschraenkingInW, setup.mindestLaufzeitInMin, setup.ausschaltTempInGradCel, setup.externerSpeicher, setup.ipInverter, setup.pid_p, setup.pid_i, setup.pid_i, setup.pid_min_time_without_contoller_inMS, setup.pid_min_time_before_switch_off_channel_inMS, setup.pid_min_time_for_dig_output_inMS, setup.pid_targetPowerInWatt, setup.pidChanged

    );
    DBGln(buffer);
}