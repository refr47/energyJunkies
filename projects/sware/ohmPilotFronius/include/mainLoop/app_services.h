#pragma once

#include <Arduino.h>

void serviceClock();
void serviceNetworkSupervisor();
void serviceTemperature();
void serviceEnergy();
void servicePid();
void serviceWeb();
void serviceMaintenance();
void serviceEpromStore(void *param);
unsigned servicePhasenSchnittBlink();
uint8_t serviceErrorBlink();