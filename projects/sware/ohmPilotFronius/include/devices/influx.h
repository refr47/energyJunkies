#pragma once
#include "defines.h"
#ifdef FEATURE_INFLUX

bool influx_init(CALLBACK_GET_DATA getData);
void influx_service();
uint32_t influx_appliedRevision();
bool influx_write(WEBSOCK_DATA &);
bool influx_write_test(double availableData, double availableDataAfter);
bool influx_write_production(double pv, double akku, double load);
bool influx_write_mean_val(double watt);
bool influx_write_mean(double mean);

#endif