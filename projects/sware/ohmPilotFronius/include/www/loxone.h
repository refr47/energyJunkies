#pragma once

#ifdef LOXONE

#include <Arduino.h>

#include "defines.h"

typedef struct {
    int error;
    int usedWatt;
    int boilerTemp;
} LOXONE_INFO;

bool loxone_init(WEBSOCK_DATA &setup);
bool loxone_sendWattUsed(WEBSOCK_DATA &); 

void loxone_prepare(WEBSOCK_DATA &webSockData,LOXONE_INFO& loxone);

#endif

/*
provide loxone virtuelle IN-/Out Register
*/