#pragma once

#include <Arduino.h>
#include "debugConsole.h"

void logReader_init();
String logReader_getBufferAsString();
void logReader_captureSerialOutput();
void logReader_enDisableRedirect(bool);