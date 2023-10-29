#pragma once
#include <Arduino.h>

#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT
#define DBGln(a) (Serial.println(a))
#define DBG1ln(a, f) (Serial.println(a, f))

#define DBG(a) (Serial.print(a))
#define DBG1(a, f) (Serial.println(a, f))
#else
#define DBGln(a)
#define DBG(a)
#define DBG1ln(a, f)
#define DBG1(a, f)
#endif