#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <ModbusIP_ESP8266.h>
#include "huawei.h"

/*
https://debacher.de/wiki/Sun2000_Modbus_Register

https://support.huawei.com/enterprise/de/doc/EDOC1100173913

Der Standardport für ModbusTCP ist 502.
https://forum.fhem.de/index.php?topic=115422.0

*/

// ip address of modbus tcp slave
static IPAddress remote;
static ModbusIP mb;

bool isConnectedAndReconnect()
{
    bool success = true;
    if (!mb.isConnected(remote))
    {
        success = mb.connect(remote);
        delay(1000);
        DBGf("modbus do connect .... %x", success);

        if (!success)
        {
            DBGf("Error in connection to Inverter via Modbus: %s", strerror(errno));
        }
    }

    return success;
}

bool mb_init(Setup &setUpData)
{

    DBGf(" Inverter Addr: %s", setUpData.ipInverterAsString);

    if (!remote.fromString(setUpData.ipInverterAsString))
    {
        DBGf("mb_init:: - cannot convert IP-Adresse of Converter from string");
        return false;
    }

    mb.client();
    return isConnectedAndReconnect();
}

bool mb_readAll(Setup &setup, MB_CONTAINER &) {
    return true;
}
bool mb_readInverterDynamic(Setup &setup, MB_CONTAINER &){
    return true;
}
bool mb_readSmartMeter(Setup &setUpData, MB_CONTAINER &){
    return true;
}
bool mb_readInverter(Setup &setUpData, MB_CONTAINER &){
    return true;
}
bool mb_readSmartMeterAndInverterOnly(Setup &setUpData, MB_CONTAINER &) {
    return true;
}
bool mb_readAkkuOnly(Setup &setUpData, MB_CONTAINER &){
    return true;
}
