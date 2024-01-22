#pragma once
#ifdef INFLUX
#include "defines.h"

bool influx_init();
bool influx_write(WEBSOCK_DATA &);

#endif