#pragma once

#include <Arduino.h>
#include "ajaxCalls.h"
void www_init(char *ipAddr, char *); // IF NULL, ACT as Access Point
void www_run();
