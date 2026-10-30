#pragma once
#define TAG_BATT "BATTERY"
#define TAG_PHASE "PHASE"
template <typename... Args>
inline void testLog(const char*, const char*, Args...) {}
#define LOG_INFO(...) testLog(__VA_ARGS__)
