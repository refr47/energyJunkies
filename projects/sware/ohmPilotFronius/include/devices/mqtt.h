#pragma once
#ifdef MQTT
bool mqtt_init();

void mqtt_publish_pidParams(double kP, double kI, double kD);
void mqtt_publish_en(int pwm, double availableWatt);

void mqtt_loop();
#endif