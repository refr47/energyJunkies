#pragma once

#include <Arduino.h>>

// modbus device id of inverter
#define INVERTER_ID 1
// modbus device id of smart meter
#define MODBUS_TCP_PORT 502
#define INVERTER_SUM_VALUE_LEN 3
#define INVERTER_STATE_VALUE_LEN 4
#define INVERTER_STRG_VALUE_LEN 7
#define METER_VALUE_LEN 3
#define DEVICE_NAME_LEN 24 // ModbusIP object

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
        double maxChargePower;   // max. charge rate in W
        double maxChargeRate;    // max. charge rate in %
        double maxDischargeRate; // max discharge rate in %
        double minReservePct;    // min. emergency reserve in %
        double stateOfCharge;    // state of charge in %
        double chargeRate;       // charge rate in %
        double dischargeRate;    // discharge rate in %
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
