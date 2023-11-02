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
#define MODBUS_WAIT_FOR_DATA_IN_MS 1000

/*
https://esp32io.com/tutorials/esp32-modbus
    ***********************************************************************
                    GLOBAL Variables
    ***********************************************************************
   */

// ip address of modbus tcp slave
static IPAddress remote;
static ModbusIP mb;

// meter modbus register array
// meter values
METER_VALUE_t meterValues;
static int16_t inverterSumRegs[INVERTER_SUM_REGS_LEN];

// meter modbus register array
static int16_t inverterStateRegs[INVERTER_STATE_REGS_LEN];
// meter values
static INVERTER_SUM_VALUE_t inverterSumValues;

// meter modbus register array
static int16_t inverterStrgRegs[INVERTER_STRG_REGS_LEN];
// meter modbus register array
static int16_t meterRegs[METER_REGS_LEN];

static AKKU_STATE_VALUE_t inverterStateValues;
static AKKU_STRG_VALUE_t inverterStrgValues;

// scaling relations for meter values
static SCALE_INDEX_t scaleInverterSum[INVERTER_SUM_VALUE_LEN] = {
    {0, 40083 - INVERTER_SUM_REGS_START, 40084 - INVERTER_SUM_REGS_START, 1}, // AC current power
    {1, 40093 - INVERTER_SUM_REGS_START, 40095 - INVERTER_SUM_REGS_START, 2}, // AC total energy exported
    {2, 40100 - INVERTER_SUM_REGS_START, 40101 - INVERTER_SUM_REGS_START, 1}  // AC total energy imported
};

// scaling relations for meter values
static SCALE_INDEX_t scaleInverterState[INVERTER_STATE_VALUE_LEN] = {
    {0, 40140 - INVERTER_STATE_REGS_START, 40141 - INVERTER_STATE_REGS_START, 1}, // storage capacity in Wh
    {1, 40144 - INVERTER_STATE_REGS_START, 40145 - INVERTER_STATE_REGS_START, 1}, // max. charge rate
    {2, 40146 - INVERTER_STATE_REGS_START, 40147 - INVERTER_STATE_REGS_START, 1}, // max. discharge rate
    {3, 40186 - INVERTER_STATE_REGS_START, -1, 4}                                 // lifetime energy
};

// scaling relations for meter values
static SCALE_INDEX_t scaleInverterStrg[INVERTER_STRG_VALUE_LEN] = {
    {0, 40345 - INVERTER_STRG_REGS_START, 40361 - INVERTER_STRG_REGS_START, 1}, // max. charge power in W
    {1, 40346 - INVERTER_STRG_REGS_START, 40362 - INVERTER_STRG_REGS_START, 1}, // max. charge rate in %
    {2, 40347 - INVERTER_STRG_REGS_START, 40362 - INVERTER_STRG_REGS_START, 1}, // max. discharge rate %
    {3, 40350 - INVERTER_STRG_REGS_START, 40364 - INVERTER_STRG_REGS_START, 1}, // min. emergency reserve %
    {4, 40351 - INVERTER_STRG_REGS_START, 40365 - INVERTER_STRG_REGS_START, 1}, // state of charge %
    {5, 40355 - INVERTER_STRG_REGS_START, 40368 - INVERTER_STRG_REGS_START, 1}, // charge rate in %
    {6, 40356 - INVERTER_STRG_REGS_START, 40368 - INVERTER_STRG_REGS_START, 1}  // discharge rate in %
};

// scaling relations for meter values
static SCALE_INDEX_t scaleMeter[METER_VALUE_LEN] = {
    {0, 40087 - METER_REGS_START, 40091 - METER_REGS_START, 1}, // AC current power
    {1, 40107 - METER_REGS_START, 40123 - METER_REGS_START, 2}, // AC total energy exported
    {2, 40115 - METER_REGS_START, 40123 - METER_REGS_START, 2}  // AC total energy imported
};

// meter modbus register array

// description of modbus register block to be read
// inverter is device id 1, storage registers start at 400345, read 24 registers, ...
// smart meter is device 200, meter registers start at 40071, read 70 registers
/*
 @see also modbusRegister.h::
    INVERTER_SUM_BLOCK_ID
    METER_BLOCK_ID
    AKKU_STATE_BLOCK_ID
    AKKU_STRG_BLOCK_ID
*/
static MODBUS_READ_t regsToRead[REG_BLOCK_COUNT] = {
    {INVERTER_SUM_BLOCK_ID, INVERTER_ID, INVERTER_SUM_REGS_START, INVERTER_SUM_REGS_LEN, "Inverter"},
    {METER_BLOCK_ID, METER_ID, METER_REGS_START, METER_REGS_LEN, "Smart Meter"},
    {AKKU_STATE_BLOCK_ID, INVERTER_ID, INVERTER_STATE_REGS_START, INVERTER_STATE_REGS_LEN, "Inverter"},
    {AKKU_STRG_BLOCK_ID, INVERTER_ID, INVERTER_STRG_REGS_START, INVERTER_STRG_REGS_LEN, "Inverter"}};

// highest number of registers to be printed (derived of regsToRead highest register number)
const int regsCount = METER_REGS_LEN; // max(max(INVERTER_SUM_REGS_LEN, INVERTER_STATE_REGS_LEN), max(INVERTER_STRG_REGS_LEN, METER_REGS_LEN));

/*
        **************************************************************************
        FUNCTIONS
        **************************************************************************

*/
bool isConnectedAndReconnect()
{
    bool success = true;
    if (!mb.isConnected(remote))
    {
        success = mb.connect(remote);
        delay(1000);
        DBG("modbus do connect ....");
        DBGln(success);
        if (!success)
        {
            DBG("Error in connection to Inverter via Modbus: ");
            DBGln(strerror(errno));
        }
    }

    return success;
}

bool mb_init(Setup &setUpData)
{

    DBG(" Inverter Addr: ");
    DBGln(setUpData.ipInverterAsString);
    DBGln(setUpData.ipInverter);

    if (!remote.fromString(setUpData.ipInverterAsString))
    {
        DBGln("mb_init:: - cannot convert IP-Adresse of Converter from string");
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
    DBGln("Modbus/TCP connected");

    transId = mb.readHreg(remote, MODBUS_COMMMON, (uint16_t *)&inverterRegs, MODBUS_STATIC_LEN, NULL, INVERTER_ID); // Initiate Read Holding Register from Modbus Slave
    DBG("transID: ");
    DBGln(transId);
    if (transId == 0)
    {

        sprintf(text, "Modbus/TCP register read failed (Device: %d, Register: %d, Count: %d)", INVERTER_ID, MODBUS_COMMMON, MODBUS_STATIC_LEN);
        DBGln(text);
        delay(5000);
        //    } else {
        //      DBGln("Modbus/TCP register read succeeded");
        //      Serial2.println("Modbus/TCP register read succeeded");
    }
    else
    {
        DBGln(" modbus done successfully ....");
    }

    mb.task(); // Common local Modbus task
    if (transId != 0)
    {

        delay(3 * MODBUS_WAIT_FOR_DATA_IN_MS); // Pulling interval
        DBG("Manufactorer: ");
        int offset = 0;
        makeString(0, MODBUS_INVERTER_MANUFACTURER_LEN, inverterRegs, &pText);
        DBGln(pText);
        DBG("Device: ");
        offset = MODBUS_INVERTER_MANUFACTURER_LEN;
        makeString(offset, offset + MODBUS_INVERTER_DEVICE_LEN, inverterRegs, &pText);
        DBGln(pText);
        DBG("SW Version: ");
        offset += MODBUS_INVERTER_MANUFACTURER_LEN;
        offset += MODBUS_INVERTER_OPTIONS;

        DBG("SW-Version: ");
        DBG(offset);
        makeString(offset, offset + MODBUS_INVERTER_SW_VERS, inverterRegs, &pText);
        DBGln(pText);

        DBGln("Done");
    }
    else
    {
        DBGln("transid is 0");
    }
    return true;
}

// index into modbus read block array
static int readIndex = 0;
// character array to prepare mesages
static char text[TEXT_LEN];

uint16_t res = 0;
int16_t resArr[REG_BLOCK_COUNT][regsCount];
float realPower;

bool mb_readInverterDynamic(Setup &setUpData, MB_CONTAINER &container)
{

    // if
    if ((setUpData.externerSpeicher == false) && (readIndex >= AKKU_STATE_BLOCK_ID))
    {
        readIndex = (readIndex + 1) % REG_BLOCK_COUNT;
        return true;
    }

    // result of modbus access functions
    uint16_t transId = 0;
    if (mb.isConnected(remote))
    { // Check if connection to Modbus Slave is established

        transId = mb.readHreg(remote, regsToRead[readIndex].baseAddr, (uint16_t *)resArr[readIndex], regsToRead[readIndex].count, NULL, regsToRead[readIndex].deviceId); // Initiate Read Holding Register from Modbus Slave
        if (transId == 0)
        {
            sprintf(text, "Modbus/TCP register read failed (Device: %d, Register: %d, Count: %d)", regsToRead[readIndex].deviceId,
                    regsToRead[readIndex].baseAddr, regsToRead[readIndex].count);
            Serial.println(text);
            delay(5000);
            //    } else {
            //      Serial.println("Modbus/TCP register read succeeded");
        }
    }
    else
    {
        bool success = mb.connect(remote); // Try to connect if no connection
        Serial.println(success ? F("Successfully connected to Modbus server") : F("Failed to connect to Modbus server"));

        if (success)
        {
            Serial.println("Modbus read susccessfully ...");
        }
        else
            return false;
    }
    mb.task(); // Common local Modbus task
    if (transId != 0)
    {
        delay(MODBUS_WAIT_FOR_DATA_IN_MS); // Pulling interval
        text[0] = '\0';                    // reset text to empty
        switch (regsToRead[readIndex].blockId)
        {
        case METER_BLOCK_ID:
            scaleValues(meterValues.value, resArr[readIndex], scaleMeter, METER_VALUE_LEN);
            sprintf(text, /*"%12s;*/ "%13.3lf;%13.3lf;%13.3lf;", meterValues.data.acCurrentPower,
                    meterValues.data.acTotalEnergyExp, meterValues.data.acTotalEnergyImp);
            memcpy(&container.meterValues.data, &meterValues.data, sizeof(meterValues.data));
            break;
        case INVERTER_SUM_BLOCK_ID:

            scaleValues(inverterSumValues.value, resArr[readIndex], scaleInverterSum, INVERTER_SUM_VALUE_LEN);
            sprintf(text, /*"%12s;*/ "%13.3lf;%13.3lf;%13.3lf;", inverterSumValues.data.acCurrentPower,
                    inverterSumValues.data.acTotalEnergy, inverterSumValues.data.dcCurrentPower);
            memcpy(&container.inverterSumValues.data, &inverterSumValues.data, sizeof(inverterSumValues.data));

        case AKKU_STATE_BLOCK_ID:
            scaleValues(inverterStateValues.value, resArr[readIndex], scaleInverterState, INVERTER_STATE_VALUE_LEN);
            sprintf(text, /*"%12s;*/ "%13.3lf;%13.3lf;%13.3lf;%13.3lf;", inverterStateValues.data.capacity,
                    inverterStateValues.data.chargeRateLimit, inverterStateValues.data.dischargeRateLimit, inverterStateValues.data.lifetimeEnergy);
            memcpy(&container.akkuState.data, &inverterSumValues.data, sizeof(inverterSumValues.data));
            break;
        case AKKU_STRG_BLOCK_ID:
            scaleValues(inverterStrgValues.value, resArr[readIndex], scaleInverterStrg, INVERTER_STRG_VALUE_LEN);
            double maxChargePower; // max. charge rate in W
            sprintf(text, /*"%12s;*/ "%13.3lf;%13.3lf;%13.3lf;%13.3lf;%13.3lf;%13.3lf;%13.3lf;",
                    inverterStrgValues.data.maxChargePower,
                    inverterStrgValues.data.maxChargeRate, inverterStrgValues.data.maxDischargeRate,
                    inverterStrgValues.data.minReservePct, inverterStrgValues.data.stateOfCharge,
                    inverterStrgValues.data.chargeRate, inverterStrgValues.data.dischargeRate);
            memcpy(&container.akkuStr.data, &inverterStrgValues.data, sizeof(inverterStrgValues.data));
            break;
        }
        if (text[0] != '\0')
        {
            DBG("Index: ");
            DBG(readIndex);
            DBGln(text);
            text[0] = '\0';
        }
        // make a line feed at the last block
        if (readIndex == REG_BLOCK_COUNT - 1)
        {
            DBGln();

            // delay(1000);
        }
        readIndex = (readIndex + 1) % REG_BLOCK_COUNT;
        return true;
    }
    else
    {
        DBGln("Modbus:: Cannot read registers ()");
        return false;
    }
}