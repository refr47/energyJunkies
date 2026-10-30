#pragma once
#include <cstdint>
enum ControlMode { MODE_OFF, MODE_LEGIONELLA, MODE_MIN_TEMP, MODE_AUTO, MODE_MANUAL };
struct LogEntry { int power = 0; uint8_t pwm = 0; uint8_t state = 0; };
