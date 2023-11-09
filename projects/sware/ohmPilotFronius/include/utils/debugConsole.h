#pragma once
#include <Arduino.h>

#define TEXTIFY(A) #A
#define ESCAPEQUOTE(A) TEXTIFY(A)

#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT

#ifndef DEBUG_PORT
#define DEBUG_PORT Serial
#endif
 
#define DBGbgn(speed) Serial.begin(speed)

#define DBGf(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)

#define DBG(format, ...) DEBUG_PORT.printf(format, ##__VA_ARGS__)

/* #define DBGln(a) (Serial.println(a))
#define DBG1ln(a, f) (Serial.println(a, f))

#define DBG(a) (Serial.print(a))
#define DBG1(a, f) (Serial.print(a, f)) */
#else
#define DBGbgn(speed)
#define DBGf(format, ...)
#define DBG(format, ...)

#endif