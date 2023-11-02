#pragma once

#include "defines.h"
#include "modbusRegister.h"
/*
gen24 symo registers

40184 40185	40186

*/
typedef struct mbContainer
{
    INVERTER_SUM_VALUE_t inverterSumValues;
    METER_VALUE_t meterValues;
    AKKU_STATE_VALUE_t akkuState;
    AKKU_STRG_VALUE_t akkuStr;

} MB_CONTAINER;

bool mb_init(Setup &);
bool mb_readInverterStatic();
bool mb_readInverterDynamic(Setup &setup, MB_CONTAINER &);
