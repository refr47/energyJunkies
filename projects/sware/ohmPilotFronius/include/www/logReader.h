#pragma once

#include <Arduino.h>
#include "debugConsole.h"

void logReader_init();
String logReader_getBufferAsString();