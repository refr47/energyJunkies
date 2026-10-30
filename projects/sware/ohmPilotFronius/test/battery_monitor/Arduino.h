#pragma once
#include <cstdint>
#include <cstdlib>
#include "debugConsole.h"
constexpr int OUTPUT = 1;
constexpr int HIGH = 1;
constexpr int LOW = 0;
unsigned long millis();
void delay(unsigned long);
void pinMode(int, int);
int digitalRead(int);
void digitalWrite(int, int);
void analogWrite(int, int);
