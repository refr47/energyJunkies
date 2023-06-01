#define __EEPROM_CPP
#include "eprom.h"
#include "tft.h"
#include <Preferences.h>

// https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/#example2

static Preferences preferences;

void eprom_storeNetwork(Network &network)
{
    preferences.begin("credentials", false);
    preferences.putString("ssid", network.ssid);
    preferences.putString("password", network.passwd);
    preferences.end();
}

void eprom_getNetwork(Network &network)
{
    preferences.begin("credentials", false);
    String ssid, passwd;
    network.ssid = "";
    network.passwd = "";
    
    ssid = preferences.getString("ssid", "").c_str();
    passwd = preferences.getString("password", "").c_str();

    if (ssid == "" || passwd == "")
    {
        Serial.println("No values saved for ssid or password");
    }
    else
    {
        network.ssid = ssid.c_str();
        network.passwd = ssid.c_str();
    }
    preferences.end();
}
