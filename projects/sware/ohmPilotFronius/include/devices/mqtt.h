#pragma once
#ifdef FEATURE_MQTT

#include "defines.h"

bool mqtt_init();

void mqtt_publish_en(int pwm, double availableWatt);
// void mqtt_publish_available_energy_with_akku(double availableWatt,double akku);

void mqtt_publish_alarm_temp(int tempS1, int tempS2);
void mqtt_publish_modbus_reconnect(const char *ip);
void mqtt_publish_modbus_wrong_production_val(double readVal);
// void mqtt_publish_modbus_current_state(MB_CONTAINER &modb);

bool mqtt_loop();
uint32_t mqtt_appliedRevision();
bool mqtt_publish_retained(const char *topic, const char *payload);
bool mqtt_publish(const char *topic, const char *payload);
using MqttMessageHandler = void (*)(void *context, const char *topic, const uint8_t *payload, unsigned length);
using MqttConnectionHandler = void (*)(void *context, bool connected, bool brokerChanged);
// Register one fixed set of incoming topics. The clock task owns all PubSubClient calls.
bool mqtt_register_receiver(const char *const *topics, unsigned count, void *context,
                            MqttMessageHandler onMessage, MqttConnectionHandler onConnection);
bool mqtt_register_load_receiver(const char *const *topics, unsigned count, void *context,
                                 MqttMessageHandler onMessage, MqttConnectionHandler onConnection);
#endif
void mqtt_publish_log(char token, const char *logM);
