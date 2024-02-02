#pragma once

#ifdef FRONIUS_API

#include "defines.h"

/*
gen24 symo registers

40184 40185	40186

*/

bool mb_init(Setup &);
bool mb_readInverterStatic();
bool mb_readAll(Setup &setup, MB_CONTAINER &);
bool mb_readInverterDynamic(Setup &setup, MB_CONTAINER &);
bool mb_readSmartMeter(Setup &setUpData, MB_CONTAINER &);
bool mb_readInverter(Setup &setUpData, MB_CONTAINER &);
bool mb_readSmartMeterAndInverterOnly(Setup &setUpData, MB_CONTAINER &);
bool mb_readAkkuOnly(Setup &setUpData, MB_CONTAINER &);

#endif