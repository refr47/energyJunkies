#pragma once
#include <stdint.h>

constexpr unsigned GRID_LOAD_COUNT = 4;

enum class GridLoadControl : uint8_t { Switch, Adjustable };

struct GridLoadConfig {
    char id[24];
    char name[40];
    uint32_t nominalW;
    uint8_t priority; // 1 is shed first
    GridLoadControl control;
};
  
struct GridLoadRuntime {
    bool fresh = false;
    bool on = false;
    bool available = false;
    bool shedByUs = false;
    bool pending = false;
    bool desiredOn = true;
    uint32_t actualW = 0;
    uint32_t shedPowerW = 0;
    uint32_t lastSeenMs = 0;
    uint32_t lastCommandMs = 0;
    uint32_t commandId = 0;
};

struct GridLoadsState {
    GridLoadRuntime loads[GRID_LOAD_COUNT];
};

const GridLoadConfig *gridLoadConfigs();
bool gridLoadsInit();
void gridLoadsEvaluate(); // called with application DATA lock held
bool gridLoadsCommandCurrent(const char *topic, const char *payload);
