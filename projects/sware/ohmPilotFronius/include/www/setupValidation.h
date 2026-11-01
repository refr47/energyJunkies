#pragma once
#include "setupFields.h"
void setupDefaults(Setup &setup);
bool validateSetup(Setup &setup, JsonArray errors);
bool applySetupPatch(Setup &setup, JsonObject patch, JsonArray errors);
