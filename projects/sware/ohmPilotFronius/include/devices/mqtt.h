#pragma once
#ifdef MQTT

#include "defines.h"

bool mqtt_init();

void mqtt_publish_pidParams(double kP, double kI, double kD);
void mqtt_publish_en(int pwm, double availableWatt);
void mqtt_publish_alarm_temp(int tempS1, int tempS2);
void mqtt_publish_modbus_reconnect(const char *ip);
void mqtt_publish_modbus_wrong_production_val(double readVal);
void mqtt_publish_modbus_current_state(MB_CONTAINER &modb);

void mqtt_loop();
#endif