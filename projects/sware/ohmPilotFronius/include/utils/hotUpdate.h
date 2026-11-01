#pragma once
#include "defines.h"
enum class SetupApplyResult { Unchanged, Applied, Retry, RestartRequired };
SetupApplyResult hotUpdate();
uint32_t setupAppliedRevision();
bool setupNeedsRestart(const Setup &current, const Setup &next);
