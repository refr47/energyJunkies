#pragma once

#include "defines.h"

typedef struct _WifiCredentials
{
    char ssid[LEN_WLAN];
    char password[LEN_WLAN];
    bool apMode;
} WifiCredentials;

void appTasks_init(bool apMode);
void appTask_setupWifoMode();
void createAppTasks(WifiCredentials &credentials);
void appTask_setupSystemConfigMode();
bool appTask_epromWriter(std::unique_ptr<Setup> setup);