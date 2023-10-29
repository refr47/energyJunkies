#pragma once

#include "defines.h"

bool pid_init(Setup *setup);
void pid_run(float availableOutputFromInverterInWatt);
