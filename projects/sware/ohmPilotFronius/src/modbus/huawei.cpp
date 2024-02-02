#ifdef HUAWEI_IV

#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <ModbusIP_ESP8266.h>
#include "huawei.h"

#include "debugConsole.h"
#include "defines.h"
/*
https://debacher.de/wiki/Sun2000_Modbus_Register

https://support.huawei.com/enterprise/de/doc/EDOC1100173913

Der Standardport für ModbusTCP ist 502.
https://forum.fhem.de/index.php?topic=115422.0

*/
#define MODBUS_WAIT_FOR_DATA_IN_MS 500
#define TEXT_LEN  512

#define REG_BLOCK_COUNT 1
#define INVERTER_SUM_BLOCK_ID 0
#define INVERTER_SUM_REGS_START 40083
#define INVERTER_SUM_REGS_LEN 19
#define INVERTER_SUM_VALUE_LEN 3

#define METER_BLOCK_ID  1
#define METER_REGS_LEN 70
#define METER_REGS_START 40071

static const int regsCount = METER_REGS_LEN;
typedef struct scaleIndex
{
    int targetIndex; // index into target values array
    int sourceIndex; // index into value registers array (value to be scaled)
    int scaleIndex;  // index into value registers array (scale factor), -1 if no scaling required
    int regCount;    // number of registers to combine into one float value
} SCALE_INDEX_t;


static MODBUS_READ_t regsToRead[REG_BLOCK_COUNT] = {
    {INVERTER_SUM_BLOCK_ID, INVERTER_ID, INVERTER_SUM_REGS_START, INVERTER_SUM_REGS_LEN, "Inverter"},
};

static SCALE_INDEX_t scaleInverterSum[INVERTER_SUM_VALUE_LEN] = {
    {0, 40083 - INVERTER_SUM_REGS_START, 40084 - INVERTER_SUM_REGS_START, 1}, // AC current power
    {1, 40093 - INVERTER_SUM_REGS_START, 40095 - INVERTER_SUM_REGS_START, 2}, // AC total energy exported
    {2, 40100 - INVERTER_SUM_REGS_START, 40101 - INVERTER_SUM_REGS_START, 1}  // AC total energy imported
};

 // scaling relations for meter values
static SCALE_INDEX_t scaleMeter[METER_VALUE_LEN] = {
    {0, 40087 - METER_REGS_START, 40091 - METER_REGS_START, 1}, // AC current power
    {1, 40107 - METER_REGS_START, 40123 - METER_REGS_START, 2}, // AC total energy exported
    {2, 40115 - METER_REGS_START, 40123 - METER_REGS_START, 2}  // AC total energy imported
};


METER_VALUE_t meterValues;


static int16_t resArr[REG_BLOCK_COUNT][regsCount];
#ifdef MODBUS_VERBOSE
// character array to prepare mesages
static char text[TEXT_LEN];
#endif

static INVERTER_SUM_VALUE_t inverterSumValues;

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

bool mb_readAll(Setup &setup, MB_CONTAINER &)
{
    return true;
}

bool mb_readSmartMeter(Setup &setUpData, MB_CONTAINER &)
{
    return true;
}
bool mb_readInverter(Setup &setUpData, MB_CONTAINER &)
{
    return true;
}
bool mb_readSmartMeterAndInverterOnly(Setup &setUpData, MB_CONTAINER &)
{
    return true;
}
bool mb_readAkkuOnly(Setup &setUpData, MB_CONTAINER &)
{
    return true;
}

static void swapRegs(uint16_t regs[], int count)
{
    uint16_t temp;
    for (int i = 0; i < count / 2; i++)
    {
        temp = regs[i];
        regs[i] = regs[count - 1 - i];
        regs[count - 1 - i] = temp;
    }
}
 

static int scaleValues(double target[], int16_t source[], SCALE_INDEX_t relation[], int count)
{
    int64_t value;

    // DBGf("scaleValues :: BEGIN and count: %d", count);
    for (int i = 0; i < count; i++)
    {
        // DBGf("Index: %d", i);
        switch (relation[i].regCount)
        {
        case 4:
        {
            REG_VALUE64_t valueUnion64;
            valueUnion64.regs[0] = source[relation[i].sourceIndex];
            valueUnion64.regs[1] = source[relation[i].sourceIndex + 1];
            valueUnion64.regs[2] = source[relation[i].sourceIndex + 2];
            valueUnion64.regs[3] = source[relation[i].sourceIndex + 3];
            // swapBytes(valueUnion64.bytes, relation[i].regCount * 2);
            swapRegs(valueUnion64.regs, relation[i].regCount);
            value = valueUnion64.value64;
            // DBGf("scaleValues (4); index: %d value: %d scaleFac: %d", i, value, relation[i].scaleIndex);
            break;
        }
        case 2:
        {
            REG_VALUE32_t valueUnion32;
            valueUnion32.regs[0] = source[relation[i].sourceIndex];
            valueUnion32.regs[1] = source[relation[i].sourceIndex + 1];
            // swapBytes(valueUnion32.bytes, relation[i].regCount * 2);
            swapRegs(valueUnion32.regs, relation[i].regCount);
            value = valueUnion32.value32;
            // value = ((uint32_t)(source[relation[i].sourceIndex + 1])) << 16;
            // value |= (uint16_t)source[relation[i].sourceIndex];
            // value = swapBytes((uint16_t)(source[relation[i].sourceIndex + 1]));
            // value = value << 16 + swapBytes(source[relation[i].sourceIndex]);
            // DBGf("scaleValues (2); index: %d value: %d scaleFac: %d", i, value, relation[i].scaleIndex);
            break;
        }
        case 1:
        default:
            value = source[relation[i].sourceIndex];
            
        }
       
      
            target[relation[i].targetIndex] = value;
       
    }
    // DBGf("scaleValues :: END and count: %d", count);
    return 0;
}


static int readIndex=0;

bool mb_readInverterDynamic(Setup &setup, MB_CONTAINER &container)
{

    uint16_t transId = 0;
    if (mb.isConnected(remote))
    { // Check if connection to Modbus Slave is established

        transId = mb.readHreg(remote, regsToRead[readIndex].baseAddr, (uint16_t *)resArr[readIndex], regsToRead[readIndex].count, NULL, regsToRead[readIndex].deviceId); // Initiate Read Holding Register from Modbus Slave
        if (transId == 0)
        {
            DBGf("Modbus/TCP register read failed (Device: %d, Register: %d, Count: %d)", regsToRead[readIndex].deviceId,
                 regsToRead[readIndex].baseAddr, regsToRead[readIndex].count);

            delay(5000);
            //    } else {
            //      Serial.println("Modbus/TCP register read succeeded");
        }
    }
    else
    {
        bool success = isConnectedAndReconnect(); // Try to connect if no connection
        // Serial.println(success ? F("Successfully connected to Modbus server") : F("Failed to connect to Modbus server"));

        if (success)
        {
            transId = mb.readHreg(remote, regsToRead[readIndex].baseAddr, (uint16_t *)resArr[readIndex], regsToRead[readIndex].count, NULL, regsToRead[readIndex].deviceId); // Initiate Read Holding Register from Modbus Slave
            DBGf("Modbus read susccessfully ...");
        }
        else
            return false;
    }
    mb.task(); // Common local Modbus task
    if (transId != 0)
    {
        delay(MODBUS_WAIT_FOR_DATA_IN_MS); // Pulling interval
#ifdef MODBUS_VERBOSE
        text[0] = '\0'; // reset text to empty
#endif
        switch (regsToRead[readIndex].blockId)
        {

        case INVERTER_SUM_BLOCK_ID:
            // inverterSumValues.data.acCurrentPower : produktion
            // DBGf("INVERTER_SUM_BLOCK_ID ");
            // DBGf("Val 1 %d".resArr[readIndex].)
               scaleValues(inverterSumValues.value, resArr[readIndex], scaleInverterSum, INVERTER_SUM_VALUE_LEN);
#ifdef MODBUS_VERBOSE            
            sprintf(text, /*"%12s;*/ "%13.3lf;%13.3lf;%13.3lf;", inverterSumValues.data.acCurrentPower,
                    inverterSumValues.data.acTotalEnergy, inverterSumValues.data.dcCurrentPower);
#endif

            memcpy(&container.inverterSumValues.data, &inverterSumValues.data, sizeof(inverterSumValues.data));
            break;
             case METER_BLOCK_ID:
            // DBGf("METER_BLOCK_ID ");
            //  meterValues.data.acCurrentPower : aktuelle einspeisung (-), Bezug: +
            scaleValues(meterValues.value, resArr[readIndex], scaleMeter, METER_VALUE_LEN);
#ifdef MODBUS_VERBOSE
            sprintf(text, /*"%12s;*/ "%13.3lf;%13.3lf;%13.3lf;", meterValues.data.acCurrentPower,
                    meterValues.data.acTotalEnergyExp, meterValues.data.acTotalEnergyImp);
#endif
            memcpy(&container.meterValues.data, &meterValues.data, sizeof(meterValues.data));
            break;
       
        }
#ifdef MODBUS_VERBOSE
        if (text[0] != '\0')
        {
            const char *cp = NULL;
            if (readIndex < 2)
                cp = readIndex == 0 ? "Inverter" : "SmartMeter";
            else
                cp = readIndex == 2 ? "Akku State" : "Akku Strg";
            DBGf("ModbusReader::Index: [%s] , data: %s", cp, text);
            text[0] = '\0';
        }
        else
        {
            DBGf("ModbusReader:: index: %d:", readIndex);
        }
#endif
        // make a line feed at the last block
        if (readIndex == REG_BLOCK_COUNT - 1)
        {
            DBGf(" ");

            // delay(1000);
        }
        readIndex = (readIndex + 1) % REG_BLOCK_COUNT;
        return true;
    }
    /* else
    {
        DBGf("Modbus:: Cannot read registers ()");
        return false;
    } */
    // DBGf("mb_readInverterDynamic EXit for readIndex: %d", readIndex);
}

#endif