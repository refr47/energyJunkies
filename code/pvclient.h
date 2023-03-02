//
// Created by ferdinand on 12.04.22.
//

#ifndef PVCLIENT_PVCLIENT_H
#define PVCLIENT_PVCLIENT_H

// power cut at 10,36kW from April to September
#define APRIL 3 // months start at 0
#define AUGUST 7 // months start at 0


// used smart meter starting register number
#define METER_ADDR 40087 // DC power scale factor onwards
#define METER_REAL_PWR (40087 - METER_ADDR)
#define METER_REAL_PWR_SF (40091 - METER_ADDR)

#define METER_REGS_COUNT (METER_REAL_PWR_SF + 1) // number of registers


// modbus unit id of inverter
#define INVERTER 1

// used inverter starting register number
#define AC_PWR_ADDR 40083 // DC power scale factor onwards
#define AC_PWR (40083 - AC_PWR_ADDR)
#define AC_PWR_SF (40084 - AC_PWR_ADDR)
#define LINE_FREQ (40085 - AC_PWR_ADDR)
#define LINE_FREQ_SF (40086 - AC_PWR_ADDR)
#define DC_PWR (40100 - AC_PWR_ADDR) // battery charge power
#define DC_PWR_SF (40101 - AC_PWR_ADDR) // DC power scale factor

#define INVRT_REGS_COUNT (DC_PWR_SF + 1) // number of registers

// used MPPT starting register number
#define MPPT_ADDR 40257 // DC power scale factor onwards
#define MPPT_PWR_SF (40257 - MPPT_ADDR)
#define MPPT_1_PWR (40274 - MPPT_ADDR)
#define MPPT_2_PWR (40294 - MPPT_ADDR)
#define MPPT_3_PWR (40314 - MPPT_ADDR) // battery charge power
#define MPPT_4_PWR (40334 - MPPT_ADDR) // battery discharge power

#define MPPT_REGS_COUNT (MPPT_4_PWR + 1) // number of registers

// used storage starting register number
#define STRG_ADDR 40348 // enable (dis-)charge limits bitfield
#define LIMIT_MODE (40348 - STRG_ADDR)
#define SOC_PERC (40351 - STRG_ADDR)
#define DIS_CHRG_PERC (40355 - STRG_ADDR)
#define CHRG_PERC (40356 - STRG_ADDR)
#define SOC_PERC_SF (40365 - STRG_ADDR)
#define CHRG_DIS_CHRG_SF (40368 - STRG_ADDR)

#define STRG_REGS_COUNT (CHRG_DIS_CHRG_SF + 1) // number of registers

#define IP_LENGTH 64
typedef struct {
    // ip address of the inverter
    char ip[IP_LENGTH];
    // modbus/tcp port
    uint16_t port;

// unifi RPi uses UTC, thus reduce hour by 1
    uint16_t cutOffDuration; // duration of power cut off (x hours)
    uint16_t cutOffBegin; // cut off begins at 11:30 MESZ, means 10:30 on local machine
    uint16_t cutOffEnd; //cut off ends at 15:00 UTC
    uint16_t progEnd; // programm should terminate 10 minutes after cut off end

    uint16_t chrgPercSlow; // limited charge rate to avoid cut off
    uint16_t chrgPercNormal; // normal charge rate
    uint16_t chrgPercMin; // minimal charge rate
    uint16_t socPercThresh; // stop charging if SoC >= xy%
    uint16_t socPercMax; // SoC upper limit

    // modbus unit id of smart meter
    uint16_t smartMeter;

    uint32_t refreshMicroSeconds; // every 30 seconds
    uint16_t elapsedLimit; // every 10th iteration (5 minutes)
    // error recovery mode
    modbus_error_recovery_mode errorRecoveryMode;
} config_t;


typedef struct {
    // state of charge
    float socPerc;
    // target state of charge according to current minute
    float socPercTarget;
    // SoC scale factor
    float socPercSf;
    // charge percent limit
    float chrgPerc;
    // charge percent limit target
    float chrgPercTarg;
    // charge percent limit calculated at cut off begin
    float chrgPercCalc;
    // charge percent scale factor
    float chrgDisChrgSf;
    // MPPT power sum
    float mpptPwr;
    // MPPT 1 power
    float mppt1Pwr;
    // MPPT 2 power
    float mppt2Pwr;
    // AC pwoer
    float acPwr;
    // DC power
    float dcPwr;
    // charge power (internally: MPPT3)
    float chrgPwr;
    // charge power (internally: MPPT4)
    float disChrgPwr;
    // MPPT power scale factor
    float mpptPwrSf;
    // smart meter power (production/consumption)
    float realPwr;
    // real power scale factor
    float realPwrSf;
    // AC power scale factor
    float acPwrSf;
    // DC power scale factor
    float dcPwrSf;
} values_t;

#endif //PVCLIENT_PVCLIENT_H
