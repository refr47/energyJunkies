#include "weather.h"
#include "utils.h"
#include "debugConsole.h"

#ifdef WEATHER_API
// Hourly radiation is the mean of the preceding hour. Request an extra day
// so the interval ending at midnight after day 3 is available as well.
// Installation: tilt 30 degrees; azimuth +35 degrees west of south
// (Open-Meteo convention: south=0, west=+90, east=-90).
static const char *PARAM = WEATHER_API "/v1/forecast?"
    "latitude=48.24&longitude=13.3208"
    "&hourly=cloud_cover,global_tilted_irradiance,temperature_2m"
    "&tilt=30&azimuth=35"
    "&timezone=Europe%2FVienna&timeformat=unixtime&forecast_days=4";

bool weather_fetch(PROGNOSE &forecast)
{
    forecast = PROGNOSE{};
    int status = 0;
    String payload = util_GET_Request(PARAM, &status);
    if (status != 200 || !weather_parse(payload.c_str(), time(nullptr), forecast)) {
        LOG_ERROR(TAG_WEATHER, "Weather unavailable or invalid (HTTP %d)", status);
        return false;
    }
    LOG_INFO(TAG_WEATHER, "PV today=%dWh remaining=%dWh tomorrow=%dWh dayAfter=%dWh ratio=%.2f bonus=%dC",
             forecast.forecastToday, forecast.forecastRemainingToday,
             forecast.forecastTomorrow, forecast.forecastDayAfterTomorrow,
             forecast.pvRatio, forecast.preheatBonus);
    return true;
}
#endif
