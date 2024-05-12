#pragma once

#include <WiFi.h>
#include "defines.h"

bool wifi_init(Setup &setup);
void wifi_scan_network();
void wifi_getLocalIP(char **pBuffer16);
bool wifi_isStillConnected();
// bool wifi_tryToReconnect(char **);
