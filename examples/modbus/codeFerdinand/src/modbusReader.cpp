

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
// modbus object, global for use in CTRL+C signal handler
static modbus_t *inverter = NULL;
static int rc = 0;
static int modbusConnectionRC = 0;

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
    {3, 40348 - INVERTER_STRG_REGS_START, 40363 - INVERTER_STRG_REGS_START, 1}, // min. charge power in W
    {4, 40349 - INVERTER_STRG_REGS_START, 40350 - INVERTER_STRG_REGS_START, 1}, // Activate hold/discharge/charge storage control mode. Bitfield value.
    {5, 40351 - INVERTER_STRG_REGS_START, 40365 - INVERTER_STRG_REGS_START, 1}, // state of charge %
    {6, 40352 - INVERTER_STRG_REGS_START, 40350 - INVERTER_STRG_REGS_START, 1}, // Currently available energy as a percent of the capacity rating.
    {7, 40355 - INVERTER_STRG_REGS_START, 40368 - INVERTER_STRG_REGS_START, 1}, // charge rate in %
    {8, 40356 - INVERTER_STRG_REGS_START, 40368 - INVERTER_STRG_REGS_START, 1}, // discharge rate in %
    {9, 40361 - INVERTER_STRG_REGS_START, 40350 - INVERTER_STRG_REGS_START, 1}  // ChaGriSet 0: PV (Charging from grid disabled)   1 : GRID(Charging from grid enabled)
};

/*
    add 40349 :RW  bit 0: CHARGEbit 1: DiSCHARGE


*/

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
const int regsCount = METER_REGS_LEN; // max(max(INVERTER_SUM_REGS_LEN, INVERTER_STATE_REGS_LEN), max(INVERTER_STRG_REGS_LEN, METER_REGS_LEN));

/*
        **************************************************************************
        FUNCTIONS
        **************************************************************************

*/

bool mb_init(char *ip, int port)
{

    DBGf(" Inverter Addr: %s", ip);

    inverter = modbus_new_tcp(ip, port);
    if (inverter == NULL)
    {
        fprintf(stderr, ": Unable to initialize Modbus slave %s at port %d\n", ip, port);
        return -1;
    }
    // set response timeout to 5.2 seconds
    modbus_set_response_timeout(inverter, 5, 200000);
    // set reconnect on failure bits, if set in command line arguments
    modbus_set_error_recovery(inverter, MODBUS_ERROR_RECOVERY_NONE);
    // connect to inverter
    rc = modbus_connect(inverter);
    if (rc == -1)
    {
        fprintf(stderr, ": Connect failed: %s\n", modbus_strerror(errno));
        modbus_close(inverter); // close failed connection??
                                // continue and retry all the time
                                // modbus_free(inverter); // stay here, don't leave
        modbusConnectionRC = -2;

        return -2;
    }
    modbusConnectionRC = 0;
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
    rc = modbus_set_slave(inverter, 1);
    if (rc == -1)
    {
        fprintf(stderr, ": Inverter set slave failed: %s\n",
                modbus_strerror(errno));
        return false;
    }
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

    if (modbusConnectionRC == 0)
    {

        // read registers
        rc = modbus_read_registers(inverter, regsToRead[readIndex].baseAddr, regsToRead[readIndex].count, (uint16_t *)resArr[readIndex]);
        if (rc == -1)
        {
            fprintf(stderr, ": Inverter read failed: %s\n",
                    modbus_strerror(errno));
            return false;
        }

        if (rc != -1)
        {

            text[0] = '\0'; // reset text to empty

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

            // make a line feed at the last block
            if (readIndex == REG_BLOCK_COUNT - 1)
            {
                DBGf(" ");

                // delay(1000);
            }
            readIndex = (readIndex + 1) % REG_BLOCK_COUNT;
            return true;
        }
        else
        {
            DBGf("Modbus:: Cannot read registers ()");
            return false;
        }
        // DBGf("mb_readInverterDynamic EXit for readIndex: %d", readIndex);
    }
    return false;
}
void mb_shutdown()
{
    modbus_close(inverter);
    modbus_free(inverter);
}