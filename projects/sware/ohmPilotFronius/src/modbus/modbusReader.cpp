#ifdef FRONIUS_IV
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
#define MODBUS_WAIT_FOR_DATA_IN_MS 500

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

static AKKU_STATE_VALUE_t akkuStateValues;
static AKKU_STRG_VALUE_t akkuStrgValues;

// scaling relations for meter values
static SCALE_INDEX_t scaleInverterSum[INVERTER_SUM_VALUE_LEN] = {
    {0, 40083 - INVERTER_SUM_REGS_START, 40084 - INVERTER_SUM_REGS_START, 1}, // AC current power
    {1, 40093 - INVERTER_SUM_REGS_START, 40095 - INVERTER_SUM_REGS_START, 2}, // AC total energy exported
    {2, 40100 - INVERTER_SUM_REGS_START, 40101 - INVERTER_SUM_REGS_START, 1}  // AC total energy imported
};

// scaling relations for meter values
static SCALE_INDEX_t akkuInverterState[INVERTER_STATE_VALUE_LEN] = {
    {0, 40140 - INVERTER_STATE_REGS_START, 40141 - INVERTER_STATE_REGS_START, 1}, // storage capacity in Wh
    {1, 40144 - INVERTER_STATE_REGS_START, 40145 - INVERTER_STATE_REGS_START, 1}, // max. charge rate
    {2, 40146 - INVERTER_STATE_REGS_START, 40147 - INVERTER_STATE_REGS_START, 1}, // max. discharge rate
    {3, 40186 - INVERTER_STATE_REGS_START, -1, 4}                                 // lifetime energy
};

// scaling relations for meter values
static SCALE_INDEX_t akkuInverterStrg[INVERTER_STRG_VALUE_LEN] = {
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
    {AKKU_STATE_BLOCK_ID, INVERTER_ID, INVERTER_STATE_REGS_START, INVERTER_STATE_REGS_LEN, "Akku"},
    {AKKU_STRG_BLOCK_ID, INVERTER_ID, INVERTER_STRG_REGS_START, INVERTER_STRG_REGS_LEN, "AkkuS"}};

// highest number of registers to be printed (derived of regsToRead highest register number)
static const int regsCount = METER_REGS_LEN; // max(max(INVERTER_SUM_REGS_LEN, INVERTER_STATE_REGS_LEN), max(INVERTER_STRG_REGS_LEN, METER_REGS_LEN));

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
        mb.client();
        LOG_DEBUG(TAG_MODBUS,"modbus do connect .... %x", success);

        if (!success)
        {
            LOG_ERROR(TAG_MODBUS,"Error in connection to Inverter via Modbus: %s", strerror(errno));
        }
    }

    return success;
}

bool mb_init(Setup &setUpData)
{
    LOG_INFO(TAG_MODBUS,"mb_init");
    LOG_DEBUG(TAG_MODBUS," Inverter Addr: %s", setUpData.inverter);

    if (!remote.fromString(setUpData.inverter))
    {
        LOG_ERROR(TAG_MODBUS,"mb_init:: - cannot convert IP-Adresse of Converter from string");
        return false;
    }

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
    LOG_DEBUG(TAG_MODBUS,"Modbus/TCP connected");

    transId = mb.readHreg(remote, MODBUS_COMMMON, (uint16_t *)&inverterRegs, MODBUS_STATIC_LEN, NULL, INVERTER_ID); // Initiate Read Holding Register from Modbus Slave

    if (transId == 0)
    {

        LOG_ERROR(TAG_MODBUS,"Modbus/TCP register read failed (Device: %d, Register: %d, Count: %d)", INVERTER_ID, MODBUS_COMMMON, MODBUS_STATIC_LEN);
        ;
        delay(5000);
        //    } else {
        //      DBGln("Modbus/TCP register read succeeded");
        //      Serial2.println("Modbus/TCP register read succeeded");
    }
    else
    {
        LOG_DEBUG(TAG_MODBUS," modbus done successfully ....");
    }

    mb.task(); // Common local Modbus task
    if (transId != 0)
    {

        delay(MODBUS_WAIT_FOR_DATA_IN_MS); // Pulling interval

        int offset = 0;
        makeString(0, MODBUS_INVERTER_MANUFACTURER_LEN, inverterRegs, &pText);
        LOG_DEBUG("Manufactor: %s", pText);

        offset = MODBUS_INVERTER_MANUFACTURER_LEN;
        makeString(offset, offset + MODBUS_INVERTER_DEVICE_LEN, inverterRegs, &pText);
        LOG_DEBUG("Device: %s", pText);

        offset += MODBUS_INVERTER_MANUFACTURER_LEN;
        offset += MODBUS_INVERTER_OPTIONS;

        makeString(offset, offset + MODBUS_INVERTER_SW_VERS, inverterRegs, &pText);

        LOG_DEBUG(TAG_MODBUS,"W-Version:: %s", pText);
    }
    else
    {
        LOG_DEBUG(TAG_MODBUS, LOG "transid is 0");
    }
    return true;
}

// index into modbus read block array
static int readIndex = 0;
#ifdef MODBUS_VERBOSE
// character array to prepare mesages
static char text[TEXT_LEN];
#endif
static uint16_t res = 0;
static int16_t resArr[REG_BLOCK_COUNT][regsCount];
static float realPower;
/*
typedef struct
{
    int blockId;                // index of modbus block
    int deviceId;               // modbus device id: inverter: 1, smart meter: 200
    int baseAddr;               // register number (already reduced by 1)
    int count;                  // number of registers to read
    char text[DEVICE_NAME_LEN]; // device name
} MODBUS_READ_t;

Structure for Device Management; blockID is currently SmartMeter
*/

bool mb_readAll(Setup &setup, MB_CONTAINER &mContainer)
{
    LOG_DEBUG(TAG_MODBUS,"modbusReader::mb_readAll");
    if (!mb_readSmartMeterAndInverterOnly(setup, mContainer))
    {
        readIndex = 0;
        return false;
    }

    if (!mb_readAkkuOnly(setup, mContainer))
    {
        readIndex = 0;
        return false;
    }
    readIndex = 0;
    return true;
}

bool mb_readSmartMeterAndInverterOnly(Setup &setUpData, MB_CONTAINER &mbContainer)
{
    // read smart meter
    if (!mb_readSmartMeter(setUpData, mbContainer))
        return false;
    // read inverter
    if (!mb_readInverter(setUpData, mbContainer))
        return false;

    return true;
}
bool mb_readSmartMeter(Setup &setUpData, MB_CONTAINER &container)
{

    int readIndexCurrent = readIndex;
    readIndex = METER_BLOCK_ID; // @see
    bool result = mb_readInverterDynamic(setUpData, container);
    readIndex = readIndexCurrent;
    return result;
}
bool mb_readInverter(Setup &setUpData, MB_CONTAINER &container)
{
    int readIndexCurrent = readIndex;
    readIndex = INVERTER_SUM_BLOCK_ID; // @see
    bool result = mb_readInverterDynamic(setUpData, container);
    readIndex = readIndexCurrent;
    return result;
}
bool mb_readAkkuOnly(Setup &setUpData, MB_CONTAINER &container)
{
    int readIndexCurrent = readIndex;
    readIndex = AKKU_STATE_BLOCK_ID; // @see
    bool result = mb_readInverterDynamic(setUpData, container);
    if (result)
    {
        readIndex = AKKU_STRG_BLOCK_ID; // @see
        result = mb_readInverterDynamic(setUpData, container);
    }
    readIndex = readIndexCurrent;
    return result;
}

bool mb_readInverterDynamic(Setup &setUpData, MB_CONTAINER &container)
{

    // DBGf("mb_readInverterDynamic ENTER for readIndex: %d", readIndex);
    //  if
    /*  if ((setUpData.externerSpeicher == false) && (readIndex >= AKKU_STATE_BLOCK_ID))
     {
         readIndex = 0; //(readIndex + 1) % REG_BLOCK_COUNT;
         return true;
     } */

    // result of modbus access functions
    uint16_t transId = 0;
    bool connected = mb.isConnected(remote);

    LOG_DEBUG(TAG_MODBUS,"mb_readInverterDynamic::MOdbus is connected  %d", connected);

    if (mb.isConnected(remote))
    { // Check if connection to Modbus Slave is established

        transId = mb.readHreg(remote, regsToRead[readIndex].baseAddr, (uint16_t *)&resArr[readIndex], regsToRead[readIndex].count, NULL, regsToRead[readIndex].deviceId); // Initiate Read Holding Register from Modbus Slave
        if (transId == 0)
        {
            LOG_ERROR(TAG_MODBUS,"mb_readInverterDynamic::Modbus/TCP register read failed (Device: %d, Register: %d, Count: %d)", regsToRead[readIndex].deviceId,
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
            transId = mb.readHreg(remote, regsToRead[readIndex].baseAddr, (uint16_t *)&resArr[readIndex], regsToRead[readIndex].count, NULL, regsToRead[readIndex].deviceId); // Initiate Read Holding Register from Modbus Slave
            LOG_DEBUG("Modbus read susccessfully ...");
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
        case AKKU_STATE_BLOCK_ID:
            scaleValues(akkuStateValues.value, resArr[readIndex], akkuInverterState, INVERTER_STATE_VALUE_LEN);
#ifdef MODBUS_VERBOSE
            sprintf(text, /*"%12s;*/ "%13.3lf;%13.3lf;%13.3lf;%13.3lf;", akkuStateValues.data.capacity,
                    akkuStateValues.data.chargeRateLimit, akkuStateValues.data.dischargeRateLimit, akkuStateValues.data.lifetimeEnergy);
#endif
            memcpy(&container.akkuState.data, &akkuStateValues.data, sizeof(akkuStateValues.data));
            break;
        case AKKU_STRG_BLOCK_ID:
            scaleValues(akkuStrgValues.value, resArr[readIndex], akkuInverterStrg, INVERTER_STRG_VALUE_LEN);
            /*   auto source = resArr[readIndex];
              DBGf("================================================");

              for (int jj = 0; jj < INVERTER_STRG_VALUE_LEN; jj++)
              {
                  int value = source[akkuInverterStrg[jj].sourceIndex];
                  DBGf("akkuStrgValues.value[%d] %d", jj, value);
              } */
            double maxChargePower; // max. charge rate in W
#ifdef MODBUS_VERBOSE
            sprintf(text, /*"%12s;*/ "%13.3lf;%13.3lf;%13.3lf;%13.3lf;%13.3lf;%13.3lf;%13.3lf;",
                    akkuStrgValues.data.maxChargePower,
                    akkuStrgValues.data.maxChargeRate, akkuStrgValues.data.maxDischargeRate,
                    akkuStrgValues.data.minReservePct, akkuStrgValues.data.stateOfCharge,
                    akkuStrgValues.data.chargeRate, akkuStrgValues.data.dischargeRate);
#endif
            memcpy(&container.akkuStr.data, &akkuStrgValues.data, sizeof(akkuStrgValues.data));
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
            DBGf("ModbusReader::Index:[%d]:: [%s] , data: %s", readIndex, cp, text);
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
            LOG_DEBUG("  ");

            // delay(1000);
        }
        readIndex = (readIndex + 1) % REG_BLOCK_COUNT;

        // bool connected = mb.isConnected(remote);
        //  DBGf("END MOdbus is connected ?????????????? %d", connected);
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