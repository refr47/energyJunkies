#pragma once

#include <Arduino.h>
#include <AsyncJson.h>
#include "debugConsole.h"
#include "defines.h"

void Serialprintln(const char *input...);
void printHWInfo();

bool isNumber(char s[]);
bool floatNum(char *s);

void ipv4_int_to_string(char *outIP, uint32_t in, bool *const success = nullptr);
uint32_t ipv4_string_to_int(char *in, bool *const success = nullptr);

bool util_checkParamFloat(const char *key, const char *argument, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, float *result);
bool util_checkParamInt(const char *key, const char *argument, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, int *result);
bool util_isFieldFilled(const char *key, const char *argument, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data);
void util_pHW();
char *util_format_Watt_kWatt(double val, char *formatBuf);
String util_GET_Request(const char *url, int *httpResponseCode);
bool utils_sock_initRestTargets(char *host, int index);
bool utils_sock_readRestTarget(WEBSOCK_DATA &, int index, GET_JSON_DATA getJson);
