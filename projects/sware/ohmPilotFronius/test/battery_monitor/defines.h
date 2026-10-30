#pragma once
#include <cstdint>
constexpr int HYSTERESIS_WATT = 12;
constexpr int MODE_AUTO = 1;
struct LogEntry {
    int power = 0;
    uint8_t pwm = 0;
    uint8_t state = 0;
};
