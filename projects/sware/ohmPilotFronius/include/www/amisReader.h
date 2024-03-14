#pragma once

#ifdef AMIS_READER_DEV

#include <Arduino.h>

#include "defines.h"

bool amisReader_initRestTargets(WEBSOCK_DATA &setup);
bool amisReader_readRestTarget(WEBSOCK_DATA &);

#endif