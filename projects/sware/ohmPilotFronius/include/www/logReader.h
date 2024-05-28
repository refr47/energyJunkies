#pragma once

#include <Arduino.h>
#include "debugConsole.h"

void logReader_init();
String logReader_getBufferAsString();
void logReader_captureSerialOutput(const char *logM);
void logReader_enDisableRedirect(bool);