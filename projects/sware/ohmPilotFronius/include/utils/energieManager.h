#pragma once
#include <Arduino.h>

void eM_setSleepTime(uint32_t time);
void eM_printWakeUpReason();
void eM_lightSleep();
void eM_deepSleep();
void eM_hibernate();

/* void eM_printFreeHeap();
void eM_printFreeStack();
void eM_printHeapFragmentation();
void eM_printHeapInfo();
void eM_printHeapStats(); */