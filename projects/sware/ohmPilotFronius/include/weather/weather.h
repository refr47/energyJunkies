#pragma once
#include <ctime>
#include <cstdint>

// Common limits also apply when WEATHER_API is disabled.
constexpr int WEATHER_PREHEAT_MAX_BONUS = 20;
constexpr int WEATHER_PREHEAT_MAX_SAFE = 80;
constexpr time_t WEATHER_DATA_MAX_AGE_SEC = 6 * 3600;
constexpr uint32_t WEATHER_FETCH_INTERVAL_MS = 6UL * 3600 * 1000;
constexpr uint32_t WEATHER_RETRY_INTERVAL_MS = 5UL * 60 * 1000;

struct PROGNOSE {
    int preheatBonus = 0;
    int forecastToday = 0;          // Full local day, Wh; used only for trend
    int forecastRemainingToday = 0; // Only future part of today's intervals, Wh
    int forecastTomorrow = 0;
    int forecastDayAfterTomorrow = 0;
    bool valid = false;
    time_t validUntil = 0;
    float cloudAverage = 0.5f;      // Tomorrow + day after tomorrow, [0,1]
    float tempOutside = 20.0f;
    float pvRatio = 1.0f;           // Today / weaker of next two days, [0,4]
};

// Pure parser/model, independent of network and GPIO; uses local timezone.
bool weather_parse(const char *payload, time_t now, PROGNOSE &forecast);
int weather_preheatBonus(int today, int remaining, int tomorrow, int dayAfter,
                         float &ratio);
#ifdef WEATHER_API
bool weather_fetch(PROGNOSE &forecast);
#endif
