#pragma once

#ifdef FRONIUS_API

#include "defines.h"

void soloar_init(Setup &setup);
bool solar_get_powerflow(FRONIUS_SOLAR_POWERFLOW &container);

#endif