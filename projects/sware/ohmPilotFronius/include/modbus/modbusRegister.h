#pragma once

#include <Arduino.h>

/* ─── Array-Größen für die Union-Structs ─────────── */

#define INVERTER_SUM_VALUE_LEN  3
#define INVERTER_STATE_VALUE_LEN 4
#define INVERTER_STRG_VALUE_LEN  7
#define METER_VALUE_LEN         3

/* ─── Wert-Datentypen ──────────────────────────────
   Diese Typen werden von defines.h::MB_CONTAINER
   consumed. Die Union-Struktur erlaubt Zugriff per
   Index (scaleValues) und per benanntem Feld.
*/

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

typedef union meterValue
{
    double value[METER_VALUE_LEN];
    struct meterData
    {
        double acCurrentPower;   // AC power in kW (neg=einspeisung, pos=bezug)
        double acTotalEnergyExp; // total AC energy exported in Wh
        double acTotalEnergyImp; // total AC energy imported in Wh
    } data;
} METER_VALUE_t;

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

/* ─── Skalierungs-Relations ──────────────────────── */

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

typedef struct scaleIndex
{
    int targetIndex; // index into target values array
    int sourceIndex; // index into source registers array
    int scaleIndex;  // index into source registers array (scale factor), -1 = none
    int regCount;    // number of registers to combine (1, 2, or 4)
} SCALE_INDEX_t;

/* ─── Skalierungsfunktion ────────────────────────── */

/**
 * @brief  Kombiniert 16-Bit-Modbus-Registern zu 32/64-Bit integers,
 *          wendet optional Skalierungsfaktor an und schreibt nach double[].
 */
int scaleValues(double target[], int16_t source[], SCALE_INDEX_t relation[], int count);
