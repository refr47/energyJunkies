#pragma once

#include <Arduino.h>

void serviceClock();
void serviceMqtt();
void serviceNetworkSupervisor();
void serviceTemperature();
void serviceEnergy();
void serviceEnergySourceRegistration();
void serviceWeather();
void servicePid();
void serviceWeb();
void serviceMaintenance();
void serviceEpromStore(void *param);
unsigned servicePhasenSchnittBlink();
uint8_t serviceErrorBlink();
