#ifndef __UTILS_H
#define __UTILS_H
#include <Arduino.h>
#include <AsyncJson.h>

void Serialprintln(const char *input...);
void printHWInfo();

bool isNumber(char s[]);
bool floatNum(char *s);

String ipv4_int_to_string(uint32_t in, bool *const success = nullptr);
uint32_t ipv4_string_to_int(String &in, bool *const success = nullptr);

bool util_checkParamFloat(const char *key, const char *argument, StaticJsonDocument<100> &data, float *result);
bool util_checkParamInt(const char *key, const char *argument, StaticJsonDocument<100> &data, int *result);
bool util_isFieldFilled(const char *key, const char *argument, StaticJsonDocument<100> &data);
void util_pHW();
#endif