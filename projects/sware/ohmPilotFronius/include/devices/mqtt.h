#pragma once
#ifdef MQTT

#include "defines.h"

bool mqtt_init();

void mqtt_publish_en(int pwm, double availableWatt);
// void mqtt_publish_available_energy_with_akku(double availableWatt,double akku);

void mqtt_publish_alarm_temp(int tempS1, int tempS2);
void mqtt_publish_modbus_reconnect(const char *ip);
void mqtt_publish_modbus_wrong_production_val(double readVal);
// void mqtt_publish_modbus_current_state(MB_CONTAINER &modb);
void mqtt_publish_log( char token,const char *logM);
 
void mqtt_loop();
#endif