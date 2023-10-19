#define __EEPROM_CPP
#include "eprom.h"
#include "tft.h"
#include <Preferences.h>

// https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/#example2

static Preferences preferences;

void eprom_storeSetup(Setup &setup)
{
    preferences.begin(CREDENTIALS, false);
    preferences.putString(_SSID, setup.ssid);
    preferences.putString(_PASSWORD, setup.passwd);
    preferences.putUInt(_HEIZPATRONE, setup.leistungHeizpatroneInW);
    preferences.putUInt(_HYSTERESE, setup.regelbereichHysterese);
    preferences.putUInt(_EINSPEISEBESCHRAENKUNG, setup.einspeiseBeschraenkingInW);
    preferences.putUInt(_MINDESTLAUFZEIT, setup.mindestLaufzeitInMin);
    preferences.putUInt(_AUSSCHALT_TEMP, setup.ausschaltTempInGradCel);
    preferences.putFloat(_PID_P, setup.pid_p);
    preferences.putFloat(_PID_I, setup.pid_i);
    preferences.putFloat(_PID_D, setup.pid_d);

    preferences.end();
}

void eprom_getSetup(Setup &setup)
{
    preferences.begin(CREDENTIALS

                      ,
                      false);
    String ssid, passwd;
    setup.ssid = "";
    setup.passwd = "";

    ssid = preferences.getString(_SSID, "").c_str();
    passwd = preferences.getString(_PASSWORD, "").c_str();

    if (ssid == "" || passwd == "")
    {
        Serial.println("No values saved for ssid or password");
    }
    else
    {
        setup.ssid = ssid.c_str();
        setup.passwd = ssid.c_str();
    }
    setup.leistungHeizpatroneInW = preferences.getUInt(_HEIZPATRONE);
    setup.regelbereichHysterese = preferences.getUInt(_HYSTERESE);

    setup.einspeiseBeschraenkingInW = preferences.getUInt(_EINSPEISEBESCHRAENKUNG);
    setup.mindestLaufzeitInMin = preferences.getUInt(_MINDESTLAUFZEIT);
    setup.ausschaltTempInGradCel = preferences.getUInt(_AUSSCHALT_TEMP);

    setup.pid_p = preferences.getFloat(_PID_P);
    setup.pid_i = preferences.getFloat(_PID_I);
    setup.pid_d = preferences.getFloat(_PID_D);

    preferences.end();
}
