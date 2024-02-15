#pragma once

#ifdef AMIS_READER_DEV

#include <Arduino.h>

#include "defines.h"

   
bool amisReader_initRestTargets(Setup &setup);
bool amisReader_readRestTarget(WEBSOCK_DATA &);

#endif