#pragma once

#ifdef SHELLY

#include "debugConsole.h"
#include "defines.h"

/*

https://shelly.guide/webhooks-https-requests/

Get status:
http://192.168.xxx.xxx/rpc/Shelly.GetStatus

Relay on:
http://192.168.xxx.xxx/relay/0?turn=on

Relay off:
http://192.168.xxx.xxx/relay/0?turn=off


*/

void shelly_init();

#endif