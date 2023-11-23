#pragma once

#include <Arduino.h>
#include "ajaxCalls.h"
bool www_init(char *ipAddr, char *, CALLBACK_GET_DATA webSockData); // IF NULL, ACT as Access Point
void www_run();
