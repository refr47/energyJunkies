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
    eprom_storeSetup(setup);
}

void eprom_test_read_Eprom()
{
    Setup setup;

    eprom_getSetup(setup);

    DBGln("  EPROM   -- Content ---");
    DBG("WLAN: ");
    DBGln(setup.ssid);
    DBG("passw: ");
    DBGln(setup.passwd);
    DBG("Heizpatr: ");
    DBGln(setup.leistungHeizpatroneInW);
    DBG("Hysterese: ");
    DBGln(setup.regelbereichHysterese);
    DBG("EinspeiseB: ");
    DBGln(setup.einspeiseBeschraenkingInW);
    DBG("MindesLZ: ");
    DBGln(setup.mindestLaufzeitInMin);
    DBG("AusschaltTemp: ");
    DBGln(setup.ausschaltTempInGradCel);
    DBG("Externer Speicher: ");
    DBGln(setup.externerSpeicher);
    bool result = true;
    DBG("IP-Inverter: ");
    String ip = ipv4_int_to_string(setup.ipInverter, &result);
    DBGln(setup.ipInverter);
    if (!result)
        DBGln("Transfer from uint to string for ip did not succeed.");
    DBGln(ip);
    DBG("PID_P: ");
    DBGln(setup.pid_p);
}