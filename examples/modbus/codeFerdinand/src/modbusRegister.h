#pragma once

/*
https://github.com/lostcontrol/wattpilot

*/
/*
// fronius inverter is device id 1, storage registers start at 400345, read 24 registers
// smart meter is device 200, meter registers start at 40087, read 5 registers
*/
/*
40084 AC power + 40085 Skalierungsfaktor
40094 AC energy + 40096 Skalierungsfaktor
40101 DC power + 40102 Skalierungsfaktor
40187 AC lifetime energy
40273 bis 40276 MPPT1-Werte
40293 bis 40296 MPPT2-Werte
40313 bis 40316 MPPT3-Werte (Laden der Batterie)
40333 bis 40336 MPPT4-Werte (Entladen der Batterie)
40352 Ladestand in % (SoC)

*/
#include "defines.h"

// modbus device id of inverter
#define INVERTER_ID 1
// modbus device id of smart meter
#define METER_ID 200

#define MODBUS_BASE 40000 // 40001 -1
// STATIC
#define MODBUS_COMMMON MODBUS_BASE + 4
#define MODBUS_INVERTER_MANUFACTURER_LEN 16
#define MODBUS_INVERTER_DEVICE_LEN 16
#define MODBUS_INVERTER_OPTIONS 8
#define MODBUS_INVERTER_SW_VERS 8

// DYNAMIC
/* #define DC_POWER 40100
#define AC_POWER 40083
#define DC_CURRENT_COMMULATED 40072 = > IDC in solar web
#define DC_CURRENT_A 40073
#define DC_CURRENT_B
#define DC_CURRENT_C 40075
#define DC_SF 40076
#define DC_POWER 40101 */

#define MODBUS_STATIC_LEN (MODBUS_INVERTER_MANUFACTURER_LEN + MODBUS_INVERTER_DEVICE_LEN + MODBUS_INVERTER_OPTIONS + MODBUS_INVERTER_SW_VERS)

// value name length
#define VALUE_NAME_LEN 24

// INVerTER SUM METER - SMARTMETER
#define INVERTER_SUM_BLOCK_ID 0
#define INVERTER_SUM_REGS_START 40083
#define INVERTER_SUM_REGS_LEN 19
#define INVERTER_SUM_VALUE_LEN 3

// meter values register definitions
#define METER_BLOCK_ID 1
#define METER_REGS_START 40071
#define METER_REGS_LEN 70
#define METER_VALUE_LEN 3

// akku summary values register definitions 1
#define AKKU_STATE_BLOCK_ID 2
#define INVERTER_STATE_REGS_START 40140
#define INVERTER_STATE_REGS_LEN 50
#define INVERTER_STATE_VALUE_LEN 4

// akku summary values register definitions 2
#define AKKU_STRG_BLOCK_ID 3
#define INVERTER_STRG_REGS_START 40345
#define INVERTER_STRG_REGS_LEN 24
#define INVERTER_STRG_VALUE_LEN 10

// akku control values

/* #define AKKU_CONTROL_BLOCK_ID 4
#define AKKU_CONTROL_REGS_START 40345
#define AKKU_CONTROL_REGS_LEN 23
#define AKKU_CONTROL_VALUE_LEN 7 */

// max. length of modbus device names
#define DEVICE_NAME_LEN 24 // ModbusIP object
// number of register blocks to read
#define REG_BLOCK_COUNT 4

// modbus device id of inverter
#define INVERTER_ID 1

// modbus device id of smart meter
#define METER_ID 200

/*
        **************************************************************************
        ADT
        **************************************************************************

*/
typedef union regValue32
{
    byte bytes[4];
    uint16_t regs[2];
    uint32_t value32;
} REG_VALUE32_t;

typedef union regValue64
{
    byte bytes[8];
    uint16_t regs[4];
    uint32_t value32[2];
    uint64_t value64;
} REG_VALUE64_t;

// scaling relations
// multiple register values are combined to one value and then scaled
// targetArray[target] = sourceArray[source] * pow(sourceArray[scale]);
typedef struct scaleIndex
{
    int targetIndex; // index into target values array
    int sourceIndex; // index into value registers array (value to be scaled)
    int scaleIndex;  // index into value registers array (scale factor), -1 if no scaling required
    int regCount;    // number of registers to combine into one float value
} SCALE_INDEX_t;

typedef union inverterSumValue
{
    double value[INVERTER_SUM_VALUE_LEN];
    struct inverterSumData
    {
        double acCurrentPower; // AC power in kW
        double acTotalEnergy;  // total AC energy produced
        double dcCurrentPower; // DC current power
    } data;
} INVERTER_SUM_VALUE_t;

typedef union akkuStateValue
{
    double value[INVERTER_STATE_VALUE_LEN];
    struct inverterStateData
    {
        double capacity;           // storage capacity in Wh
        double chargeRateLimit;    // max. charge rate
        double dischargeRateLimit; // max discharge rate
        double lifetimeEnergy;     // lifetime energy stored
    } data;
} AKKU_STATE_VALUE_t;

// meter values union consisting of float array and float structure components
typedef union akkuStrgValue
{
    double value[INVERTER_STRG_VALUE_LEN];
    struct akkuStrgData
    {
        double maxChargePower;      // max. charge rate in W
        double maxChargeRate;       // max. charge rate in %
        double maxDischargeRate;    // max discharge rate in %
        double minReservePct;       // min. emergency reserve in %
        double activeHoldCharge;    // active hold charge in W
        double stateOfCharge;       // state of charge in %
        double currentAvailableMem; // current available memory in %
        double chargeRate;          // charge rate in %
        double dischargeRate;       // discharge rate in %
        double chargeGridSet;
    } data;
} AKKU_STRG_VALUE_t;

// meter values union consisting of float array and float structure components
typedef union meterValue
{
    double value[METER_VALUE_LEN];
    struct meterData
    {
        double acCurrentPower;   // AC power in kW
        double acTotalEnergyExp; // total AC energy exported in Wh
        double acTotalEnergyImp; // total AC energy imported in Wh
    } data;
} METER_VALUE_t;

typedef struct
{
    int blockId;                // index of modbus block
    int deviceId;               // modbus device id: inverter: 1, smart meter: 200
    int baseAddr;               // register number (already reduced by 1)
    int count;                  // number of registers to read
    char text[DEVICE_NAME_LEN]; // device name
} MODBUS_READ_t;

typedef struct mbContainer
{
    INVERTER_SUM_VALUE_t inverterSumValues;
    METER_VALUE_t meterValues;
    AKKU_STATE_VALUE_t akkuState;
    AKKU_STRG_VALUE_t akkuStr;

} MB_CONTAINER;

// scale values: set target to source * scale factor, number of elements
int scaleValues(double target[], int16_t source[], SCALE_INDEX_t relation[], int count);

// swap bytes of an byte array
void swapBytes(byte bytes[], int count);

// swap bytes of an byte array
void swapRegs(uint16_t regs[], int count);

u8_t getHighByte(uint16_t b);
u8_t getLowByte(uint16_t b);
void makeString(int indexF, int indexT, int16_t *regArr, char **stringBase);
