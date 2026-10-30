#include "weather.h"
#include "thermalController.h"
#include "mlPredictor.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

static unsigned long ticks = 0;
unsigned long millis() { return ticks; }
time_t date(int year, int month, int day, int hour = 0) {
    tm t{};
    t.tm_year = year - 1900; t.tm_mon = month - 1; t.tm_mday = day;
    t.tm_hour = hour; t.tm_isdst = -1;
    return mktime(&t);
}

JsonDocument fixture(time_t midnight) {
    JsonDocument doc;
    auto h = doc["hourly"].to<JsonObject>();
    auto times = h["time"].to<JsonArray>();
    auto radiation = h["shortwave_radiation"].to<JsonArray>();
    auto clouds = h["cloud_cover"].to<JsonArray>();
    auto temperatures = h["temperature_2m"].to<JsonArray>();
    tm first{}; localtime_r(&midnight, &first);
    for (int i = 0; i < 96; ++i) {
        time_t end = midnight + i * 3600;
        time_t midpoint = end - 1800;
        tm t{}; localtime_r(&midpoint, &t);
        bool today = t.tm_yday == first.tm_yday && t.tm_year == first.tm_year;
        times.add(static_cast<int64_t>(end));
        radiation.add(t.tm_hour >= 9 && t.tm_hour < 17 ? (today ? 600 : 100) : 0);
        clouds.add(50); temperatures.add(15);
    }
    return doc;
}
bool parse(JsonDocument &doc, time_t now, PROGNOSE &p) {
    std::string json; serializeJson(doc, json);
    return weather_parse(json.c_str(), now, p);
}

void weatherTests() {
    float ratio = 0;
    assert(weather_preheatBonus(20000, 6000, 2000, 2000, ratio) == 20);
    assert(weather_preheatBonus(20000, 6000, 25000, 2000, ratio) == 20);
    assert(weather_preheatBonus(20000, 6000, 25000, 30000, ratio) == 0);
    assert(weather_preheatBonus(20000, 0, 2000, 2000, ratio) == 0);
    assert(weather_preheatBonus(20000, 6000, 0, 0, ratio) == 20);
    assert(weather_preheatBonus(0, 0, 0, 0, ratio) == 0);
    assert(weather_preheatBonus(2000, 100, 1000, 1000, ratio) <
           weather_preheatBonus(20000, 10000, 10000, 10000, ratio));
    // Ordinary date, both DST changes, and New Year: no fixed 24h indexing.
    const time_t days[] = {date(2026, 10, 30), date(2026, 3, 29),
                          date(2026, 10, 25), date(2026, 12, 31)};
    for (time_t day : days) {
        auto doc = fixture(day);
        PROGNOSE p;
        assert(parse(doc, day + 12 * 3600, p));
        assert(p.valid && p.preheatBonus == 20);
        assert(p.forecastRemainingToday > 0 && p.forecastRemainingToday < p.forecastToday);
        assert(p.forecastTomorrow > 0 && p.forecastTomorrow == p.forecastDayAfterTomorrow);
        assert(std::abs(p.cloudAverage - 0.5f) < 0.001f);
        assert(parse(doc, day + 21 * 3600, p));
        assert(p.preheatBonus == 0 && p.forecastRemainingToday == 0);
        assert(p.validUntil - (day + 21 * 3600) <= 4 * 3600);
        doc["hourly"]["shortwave_radiation"][12] = nullptr;
        assert(!parse(doc, day + 12 * 3600, p) && !p.valid);
    }
    PROGNOSE p;
    auto doc = fixture(days[0]);
    doc["hourly"]["time"][20] = doc["hourly"]["time"][19];
    assert(!parse(doc, days[0], p));
    doc = fixture(days[0]); doc["hourly"].remove("cloud_cover");
    assert(!parse(doc, days[0], p));
    assert(!weather_parse("{}", days[0], p));
    assert(!weather_parse("not JSON", days[0], p));
    doc = fixture(days[0]); assert(!parse(doc, 0, p));
}

void thermalTests() {
    bool active = false, ml = false;
    unsigned long last = 0;
    ControlMode mode;
    assert(ThermalController::mustStopHeating(-127, -127));
    assert(ThermalController::mustStopHeating(80, 30));
    assert(ThermalController::mustStopHeating(30, 80));
    assert(!ThermalController::mustStopHeating(-127, 60));
    assert(!ThermalController::mustStopHeating(79, 75));
    assert(ThermalController::resolveInputTemp(-127, -127, 60, 30) == -1);
    assert(ThermalController::resolveInputTemp(-127, 50, 60, 30) == 50);
    ThermalController::evaluateMode(-1, 80, 30, 65, 24, 100, active, last, mode, ml);
    assert(mode == MODE_OFF);
    ThermalController::evaluateMode(70, 80, 30, 0, 0, 100, active, last, mode, ml);
    assert(mode == MODE_AUTO);
    ThermalController::evaluateMode(80, 100, 30, 90, 1, 4000000, active, last, mode, ml);
    assert(mode == MODE_OFF && !active);
    ThermalController::evaluateMode(50, 60, 30, 65, 1, 4000000, active, last, mode, ml);
    assert(mode == MODE_LEGIONELLA && active);
    ThermalController::evaluateMode(62, 60, 30, 65, 1, 4002000, active, last, mode, ml);
    assert(mode == MODE_LEGIONELLA && active);
    ThermalController::evaluateMode(65, 60, 30, 65, 1, 4004000, active, last, mode, ml);
    assert(mode == MODE_OFF && !active && last == 4004000);
    ThermalController::evaluateMode(50, 60, 30, 65, 1, 4006000, active, last, mode, ml);
    assert(mode == MODE_AUTO && !active);
}

void mlTests() {
    MlPredictor predictor;
    float preheat, buffer;
#ifdef TINYNN_ENABLE
    Preferences::clear();
    TinyNN nn(4500, 30, 60);
    predictor.setTinyNN(&nn);
#endif
    assert(predictor.temperatureBonus(20, false, 50, 60, 30, 4, .9, 5, 0, preheat, buffer) == 0);
    assert(predictor.temperatureBonus(20, true, 50, 60, 30, 4, .9, 5, 0, preheat, buffer) == 20);
    for (int i = 1; i < 100; ++i)
        assert(predictor.temperatureBonus(20, true, 50, 60, 30, 4, .9, 5, i * 2000, preheat, buffer) == 20);
#ifdef TINYNN_ENABLE
    assert(nn.sampleCount() == 1);
    assert(preheat > 0 && preheat < 1 && buffer > 0 && buffer < 1);
    assert(Preferences::writes() == 0);
    // Actual neural implementation must learn an external target, not its own output.
    TinyNNExperience e{}; e.temp = .1f; e.pv_ratio = .5f;
    e.cloud_avg = .7f; e.temp_outside = -.2f;
    e.target_preheat = .95f; e.target_buffer = .1f;
    auto before = nn.predict(e.temp, e.pv_ratio, e.cloud_avg, e.temp_outside);
    for (int i = 0; i < 1000; ++i) { nn.remember(e); nn.trainReplay(); }
    auto after = nn.predict(e.temp, e.pv_ratio, e.cloud_avg, e.temp_outside);
    assert(std::abs(after.preheat_score - e.target_preheat) < std::abs(before.preheat_score - e.target_preheat));
    assert(std::abs(after.buffer_pct - e.target_buffer) < std::abs(before.buffer_pct - e.target_buffer));
    assert(Preferences::writes() == 0);
    ticks = 6UL * 3600 * 1000; nn.trainReplay();
    assert(Preferences::writes() == 5);
    nn.trainReplay(); assert(Preferences::writes() == 5);
    TinyNN restored(4500, 30, 60);
    assert(restored.sampleCount() == nn.sampleCount());
    const int adjusted = predictor.temperatureBonus(10, true, 50, 60, 30, 2, .9, 5, ticks, preheat, buffer);
    assert(adjusted >= 8 && adjusted <= 12);
#endif
    assert(predictor.temperatureBonus(0, true, 50, 60, 30, 1, .9, 5, 200000, preheat, buffer) == 0);
}
int main() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); tzset();
    weatherTests(); thermalTests(); mlTests();
    std::cout << "Weather, thermal and ML regression tests passed\n";
}
