#ifndef __MODBUS_REGISTER__H__
#define __MODBUS_REGISTER__H__

/*
https://github.com/lostcontrol/wattpilot

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
#define INVERTER 1
#define MODBUS_BASE 40000 // 40001 -1
// STATIC
#define MODBUS_COMMMON MODBUS_BASE + 4
#define MODBUS_INVERTER_MANUFACTURER_LEN 16
#define MODBUS_INVERTER_DEVICE_LEN 16
#define MODBUS_INVERTER_OPTIONS 8
#define MODBUS_INVERTER_SW_VERS 8

// DYNAMIC
#define DC_POWER 40100
#define AC_POWER 40083 // PAC  AC POwer negativ if consuming
#define DC_CURRENT_COMMULATED 40072 = > IDC in solar web
#define DC_CURRENT_A 40073
#define DC_CURRENT_B
#define DC_CURRENT_C 40075
#define DC_SF 40076
#define DC_POWER 40101

#define MODBUS_STATIC_LEN (MODBUS_INVERTER_MANUFACTURER_LEN + MODBUS_INVERTER_DEVICE_LEN + MODBUS_INVERTER_OPTIONS + MODBUS_INVERTER_SW_VERS)

#endif