#include "weather.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>

namespace {
// Existing installation rating. Open-Meteo supplies irradiation on the
// configured module plane (30 degrees tilt, 35 degrees west of south).
// Do not apply a second orientation correction here.
constexpr float ARRAY_WP = 405.0f * 36;
constexpr float INVERTER_W = 10000.0f;
float pvPower(float radiation, float ambient)
{
    const float cell = ambient + radiation / 800.0f * (45.0f - 20.0f);
    const float power = ARRAY_WP * radiation / 1000.0f *
                        (1.0f - 0.0035f * (cell - 25.0f)) * 0.88f;
    return std::max(0.0f, std::min(INVERTER_W, power));
}

bool number(JsonVariantConst v, float low, float high, float &value)
{
    if (!v.is<float>()) return false;
    value = v.as<float>();
    return std::isfinite(value) && value >= low && value <= high;
}
}

int weather_preheatBonus(int today, int remaining, int tomorrow, int dayAfter,
                         float &ratio)
{
    const int weakerDay = std::min(tomorrow, dayAfter);
    ratio = std::min(4.0f, std::max(0.0f,
        static_cast<float>(today) / std::max(10, weakerDay)));
    if (today <= 0 || remaining <= 0 || ratio <= 1.0f) return 0;
    // Full reserve at a 2.5:1 deterioration. Fade it when little energy is
    // left today; 2 kWh is a planning heuristic, not a measured heat demand.
    const float shortage = std::min(1.0f, (ratio - 1.0f) / 1.5f);
    const float opportunity = std::min(1.0f, remaining / 2000.0f);
    return static_cast<int>(std::lround(WEATHER_PREHEAT_MAX_BONUS * shortage * opportunity));
}

bool weather_parse(const char *payload, time_t now, PROGNOSE &forecast)
{
    forecast = PROGNOSE{};
    if (!payload || now < 1704067200) return false; // clock not synchronized
    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;
    const JsonObjectConst hourly = doc["hourly"].as<JsonObjectConst>();
    const JsonArrayConst times = hourly["time"].as<JsonArrayConst>();
    const JsonArrayConst radiation = hourly["global_tilted_irradiance"].as<JsonArrayConst>();
    const JsonArrayConst clouds = hourly["cloud_cover"].as<JsonArrayConst>();
    const JsonArrayConst temperatures = hourly["temperature_2m"].as<JsonArrayConst>();
    const size_t count = times.size();
    if (count < 72 || count > 100 || radiation.size() != count ||
        clouds.size() != count || temperatures.size() != count) return false;

    // Build calendar boundaries with mktime, including 23/25-hour DST days.
    tm local{};
    localtime_r(&now, &local);
    local.tm_hour = local.tm_min = local.tm_sec = 0;
    local.tm_isdst = -1;
    time_t boundary[4];
    boundary[0] = mktime(&local);
    for (int day = 1; day < 4; ++day) {
        local.tm_mday++;
        local.tm_isdst = -1;
        boundary[day] = mktime(&local);
    }
    double energy[3] = {}, coverage[3] = {};
    double remaining = 0, cloudSum = 0, tempSum = 0, featureHours = 0;
    time_t previous = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!times[i].is<int64_t>()) return false;
        const time_t end = times[i].as<int64_t>();
        if (i && end - previous != 3600) return false;
        previous = end;
        const time_t start = end - 3600;
        if (end <= boundary[0] || start >= boundary[3]) continue;
        float g, cc, temp;
        if (!number(radiation[i], 0, 1600, g) ||
            !number(clouds[i], 0, 100, cc) ||
            !number(temperatures[i], -80, 65, temp)) return false;
        // Radiation already contains the weather's effect, including diffuse
        // light. Do not multiply it by sunshine duration or cloud cover again.
        const float power = pvPower(g, temp);
        for (int day = 0; day < 3; ++day) {
            const time_t overlap = std::max<time_t>(0,
                std::min(end, boundary[day + 1]) - std::max(start, boundary[day]));
            const double hours = overlap / 3600.0;
            coverage[day] += overlap;
            energy[day] += power * hours;
            if (day == 0) {
                const time_t future = std::max<time_t>(0,
                    std::min(end, boundary[1]) - std::max(start, now));
                remaining += power * (future / 3600.0);
            } else {
                cloudSum += cc * hours;
                tempSum += temp * hours;
                featureHours += hours;
            }
        }
    }
    for (int day = 0; day < 3; ++day) {
        if (coverage[day] != boundary[day + 1] - boundary[day]) return false;
    }
    forecast.forecastToday = std::lround(energy[0]);
    forecast.forecastRemainingToday = std::lround(remaining);
    forecast.forecastTomorrow = std::lround(energy[1]);
    forecast.forecastDayAfterTomorrow = std::lround(energy[2]);
    forecast.cloudAverage = cloudSum / featureHours / 100.0;
    forecast.tempOutside = tempSum / featureHours;
    forecast.preheatBonus = weather_preheatBonus(forecast.forecastToday,
        forecast.forecastRemainingToday, forecast.forecastTomorrow,
        forecast.forecastDayAfterTomorrow, forecast.pvRatio);
    forecast.validUntil = std::min(now + WEATHER_DATA_MAX_AGE_SEC, boundary[1]);
    forecast.valid = true;
    return true;
}
