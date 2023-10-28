#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <ModbusIP_ESP8266.h>
#include <Arduino.h>
#include "modbusReader.h"
#include "modbusRegister.h"
#include "defines.h"

/*
    *********************************************************
 DEFINES
 ************************************************************
           */

#define TEXT_LEN 256

#define INVERTER_STRG_REGS_LEN 28

// modbus device id of inverter
#define INVERTER_ID 1

// modbus device id of smart meter
#define METER_ID 200

#define MOD_BASE_REG 40083 // ac power
#define MOD_BASE_REG_COUNT 10

/*
https://esp32io.com/tutorials/esp32-modbus
    ***********************************************************************
 GLOBAL Variables
    ***********************************************************************
   */

// ip address of modbus tcp slave
IPAddress remote;
ModbusIP mb;

// meter modbus register array

u8_t getHighByte(uint16_t b)
{
    return (b >> 8) & 0xff;
}
u8_t getLowByte(uint16_t b)
{
    return b & 0xff;
}

void makeString(int indexF, int indexT, int16_t *regArr, char **stringBase)
{
    char *base = *stringBase;
    for (int jj = indexF; jj < indexT; jj++)
    {
        if (regArr + jj == 0)
            return;
        *base = getHighByte(regArr[jj]);
        if (*base == 0)
            return;
        *(++base) = 0;
        *base = getLowByte(regArr[jj]);
        if (*base == 0)
            return;
        *(++base) = 0;
    }
}

bool isConnectedAndReconnect()
{
    bool success = true;
    if (!mb.isConnected(remote))
    {
        success = mb.connect(remote);
        Serial.print("modbus do connect ....");
        Serial.println(success);
        Serial.println(strerror(errno));
    }

    return success;
}

bool mb_init(Setup &setup)
{
    Serial.print(" Inverter Addr: ");
    Serial.println(setup.ipInverterAsString);
    Serial.println(setup.ipInverter);

    if (!remote.fromString(setup.ipInverterAsString))
    {
        Serial.println("mb_init:: - cannot convert IP-Adresse of Converter from string");
        return false;
    }

    mb.client();
    return isConnectedAndReconnect();
}

bool mb_readInverterStatic()
{
    char text[TEXT_LEN];
    int16_t inverterRegs[MODBUS_STATIC_LEN + 1];
    char *pText = text;
    uint16_t transId = 0;
    if (!isConnectedAndReconnect())
        return false;
    Serial.println("Modbus/TCP connected");

    transId = mb.readHreg(remote, MODBUS_COMMMON, (uint16_t *)&inverterRegs, MODBUS_STATIC_LEN, NULL, INVERTER_ID); // Initiate Read Holding Register from Modbus Slave
    Serial.print("transID: ");
    Serial.println(transId);
    if (transId == 0)
    {

        sprintf(text, "Modbus/TCP register read failed (Device: %d, Register: %d, Count: %d)", INVERTER_ID, MOD_BASE_REG, MOD_BASE_REG_COUNT);
        Serial.println(text);
        delay(5000);
        //    } else {
        //      Serial.println("Modbus/TCP register read succeeded");
        //      Serial2.println("Modbus/TCP register read succeeded");
    }
    else
    {
        Serial.println(" modbus done successfully ....");
    }

    mb.task(); // Common local Modbus task
    if (transId != 0)
    {

        /*  Serial.println(inverterRegs.manufactor);*/
        /*    for (int jj = 0; jj < MODBUS_COMMMON_LEN; jj++)
           {
               Serial.print(jj);
               Serial.print(": ");
               Serial.println(inverterRegs[jj], HEX);
           }
    */
        Serial.print("Manufactorer: ");
        int offset = 0;
        makeString(0, MODBUS_INVERTER_MANUFACTURER_LEN, inverterRegs, &pText);
        Serial.println(pText);
        Serial.print("Device: ");
        offset = MODBUS_INVERTER_MANUFACTURER_LEN;
        makeString(offset, offset + MODBUS_INVERTER_DEVICE_LEN, inverterRegs, &pText);
        Serial.println(pText);
        Serial.print("SW Version: ");
        offset += MODBUS_INVERTER_MANUFACTURER_LEN;
        offset += MODBUS_INVERTER_OPTIONS;

        Serial.print("SW-Version: ");
        Serial.print(offset);
        makeString(offset, offset + MODBUS_INVERTER_SW_VERS, inverterRegs, &pText);
        Serial.println(pText);

        Serial.println("Done");
    }
    else
    {
        Serial.println("transid is 0");
    }
    return true;
}
