#ifndef _MODBUS_READER
#define _MODBUS_READER
#include "defines.h"
/*
gen24 symo registers

40184 40185	40186

*/

bool mb_init(Setup &);
bool mb_readInverterStatic();

#endif