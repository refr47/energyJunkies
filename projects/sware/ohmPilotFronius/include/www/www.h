#pragma once

#include <Arduino.h>
#include "ajaxCalls.h"
bool www_init(Setup &setupData,char *ipAddr, char *, CALLBACK_GET_DATA webSockData, CALLBACK_SET_SETUP_CHANGED setSetupChanged); // IF NULL, ACT as Access Point
void www_run();
