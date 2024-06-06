#pragma once

#ifdef SHELLY
#include <Arduino.h>
#include <WiFiUdp.h>
#include "debugConsole.h"
#include "defines.h"
#include "utils.h"

#define TROCKNER_shellyIndex 0
#define POOL_PUMPE_shellyIndex 1
#define POOL_WPUMPE_shellyIndex 2

#define TROCKNER_SHELLY_ID 10
#define POOL_PUMPE_SHELLY_ID 11
#define POOL_WPUMPE_SHELLY_ID 12

#define SHELLY_DEVICES POOL_WPUMPE_shellyIndex + 1
#define ERROR_CONTAINER_LEN 128
#define METHODE_LEN 64

#define SHELLY_SWITCH_METHOD "Switch.Set"
#define SHELLY_STATUS_METHOD "Shelly.GetStatus"
#define SHELLY_RESET_METHOD "Switch.ResetCounters"

/*

https://shelly.guide/webhooks-https-requests/

Get status:
http://192.168.xxx.xxx/rpc/Shelly.GetStatus

Relay on:
http://192.168.xxx.xxx/relay/0?turn=on

Relay off:
http://192.168.xxx.xxx/relay/0?turn=off



*/

void shelly_init(SHELLY_OBJ *shellyObj);
bool shelly_switchOnOff(bool onFF, unsigned int sIndex);
bool shelly_getStatus(unsigned int sIndex);
bool shelly_resetShelly(unsigned int sIndex);
#endif