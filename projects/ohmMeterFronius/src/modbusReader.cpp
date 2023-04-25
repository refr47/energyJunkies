#include <ModbusIP_ESP8266.h>
#include "modbusReader.h"

/*             DEFINES                      */

#define TEXT_LEN 256

#define INVERTER_STRG_REGS_LEN 28

// modbus device id of inverter
#define INVERTER_ID 1

// modbus device id of smart meter
#define METER_ID 200
#define GEN24 "172.17.19.201"

#define MOD_BASE_REG 40083 // ac power
#define MOD_BASE_REG_COUNT 10

/*          GLOBAL Variables                      */

// ip address of modbus tcp slave
// IPAddress remote(GEN24, IP, v4, Adresse);
IPAddress remote(172, 17, 68, 201);
// ModbusIP object
ModbusIP mb;

// meter modbus register array
int16_t inverterStrgRegs[INVERTER_STRG_REGS_LEN];

bool mb_init()
{
    mb.client();
    return true;
}

bool mb_readInverter()
{
    char text[TEXT_LEN];
    uint16_t transId = 0;
    if (mb.isConnected(remote))
    { // Check if connection to Modbus Slave is established
        // Serial.println("Modbus/TCP connected");
        // Serial2.println("Modbus/TCP connected");
        // mb.readHreg(remote, REG, &res);  // Initiate Read Coil from Modbus Slave
        transId = mb.readHreg(remote, MOD_BASE_REG, (uint16_t *)inverterStrgRegs, MOD_BASE_REG_COUNT, NULL, INVERTER_ID); // Initiate Read Holding Register from Modbus Slave
        if (transId == 0)
        {
            sprintf(text, "Modbus/TCP register read failed (Device: %d, Register: %d, Count: %d)", INVERTER_ID, MOD_BASE_REG, MOD_BASE_REG_COUNT);
            Serial.println(text);
            Serial2.println(text);
            delay(5000);
            //    } else {
            //      Serial.println("Modbus/TCP register read succeeded");
            //      Serial2.println("Modbus/TCP register read succeeded");
        }
    }
    else
    {
        bool success = mb.connect(remote); // Try to connect if no connection
        Serial.println(success ? F("Successfully connected to Modbus server") : F("Failed to connect to Modbus server"));
        Serial2.println(success ? F("Successfully connected to Modbus server") : F("Failed to connect to Modbus server"));
        if (success)
        {
            // drawHeader();
            Serial.println("Modbus connected to inverter");
        }
    }
    mb.task(); // Common local Modbus task
    if (transId != 0)
    {
        delay(1000);    // Pulling interval
        text[0] = '\0'; // reset text to empty

        if (text[0] != '\0')
        {
            Serial.print(text);
            Serial2.print(text);
        }
    }
    return true;
}
