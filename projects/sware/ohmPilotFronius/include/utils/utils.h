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

bool util_checkParamFloat(const char *key, const char *argument, DynamicJsonDocument &data, float *result);
bool util_checkParamInt(const char *key, const char *argument, DynamicJsonDocument &data, int *result);
bool util_isFieldFilled(const char *key, const char *argument, DynamicJsonDocument &data);
void util_pHW();
char *util_format_Watt_kWatt(double val, char *formatBuf);
String util_GET_Request(const char *url, int *httpResponseCode);
bool utils_sock_initRestTargets(Setup &setupData, int index);
bool utils_sock_readRestTarget(WEBSOCK_DATA &, int index, GET_JSON_DATA getJson);
void utils_logWrite(RingBuffer &rb, const LogEntry &e);
bool utils_logRead(RingBuffer &rb, LogEntry &out);
bool utils_shouldLog(bool l1, bool l2, uint8_t pwm, bool legionella, bool minTemp);
char* utils_floatToString(float value);

/**
 * Konvertiert float sicher in einen String.
 * @param val Der Fließkommawert
 * @param width Mindestbreite (inkl. Punkt)
 * @param precision Nachkommastellen
 * @param buf Zielpuffer (muss groß genug sein!)
 * @return Zeiger auf den Puffer
 */
inline char *fToStr(double val, signed char width, unsigned char precision, char *buf)
{
    // Falls der Wert ungültig ist (NaN/Inf), fangen wir das ab
    if (isnan(val))
        return (char *)"NaN";
    if (isinf(val))
        return (char *)"Inf";

    // dtostrf ist auf dem ESP32 viel schneller/stack-schonender als sprintf
    return dtostrf(val, width, precision, buf);
}