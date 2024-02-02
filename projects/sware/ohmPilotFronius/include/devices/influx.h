#pragma once
#ifdef INFLUX
#include "defines.h"

bool influx_init();
bool influx_write(WEBSOCK_DATA &);
bool influx_write_test(double availableData, double availableDataAfter, WEBSOCK_DATA &);
bool influx_write_production(double pv, double akku, double load);
bool influx_write_mean_val(double watt);
bool influx_write_mean(double mean);

#endif