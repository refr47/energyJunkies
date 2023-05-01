#ifndef _WIFI_H__
#define _WIFI_H__

#include <WiFi.h>

#define WIFI_RECONNECT_START "Reconnect"
#define WIFI_RECONNECT_DONE "Connected"
#define WIFI_RECONNECT_FALSE "Not Connected"

bool wifi_init(); 
void wifi_scan_network();
void wifi_getLocalIP(char **pBuffer16);
bool wifi_isStillConnected();
bool wifi_tryToReconnect(char **);

#endif
