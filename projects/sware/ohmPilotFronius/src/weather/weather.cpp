#include "weather.h"
#include "utils.h"
#include "curTime.h"
#include "debugConsole.h"

#include <Arduino_JSON.h>

#ifdef WEATHER_API

#define HOST_NAME WEATHER_API
#define PATH_NAME_FORECAST "/v1/forecast"

const char *PARAM = "https://api.open-meteo.com/v1/forecast?"
    "latitude=48.24&longitude=13.3208"
    "&hourly=cloudcover,shortwave_radiation,precipitation,sunshine_duration,temperature_2m"
    "&timezone=Europe%2FBerlin&forecast_days=2";

#define JSON_ARRAY_SIZE 8500

/* ─── PV-Anlage (Nutzer-Parameter) ─────────────────────────────────── */
const float P_stc            = 405.0;   // W pro Modul bei STC
const int   N_mod            = 36;      // Anzahl Module
const float NOCT             = 45.0;    // °C, nominal operating cell temp
const float temp_coeff       = -0.0035; // -0.35%/K
const float system_loss      = 0.88;    // Gesamtverluste
const float inv_max_AC       = 10000.0; // Wechselrichter AC
const float tilt             = 30.0;    // Modulneigung [°]
const float azimut           = 159.0;   // Azimut (0=Süd, 159=Südwest)
const float tz_lon           = 15.0;    // Standardmeridian MEZ

// Risikoparameter
const float k_risk = 0.5;

static DynamicJsonDocument doc(JSON_ARRAY_SIZE);

/* ─── Hilfsfunktionen (astronomisch) ──────────────────────────────── */
static float deg2rad(float d) { return d * PI / 180.0f; }

static float equationOfTime(int n)
{
    float B = deg2rad((360.0f / 365.0f) * (n - 81));
    return 9.87f * sin(2 * B) - 7.53f * cos(B) - 1.5f * sin(B);
}

static float solarTime(float localHour, int dayOfYear, float longitude, float tzLon)
{
    float eot    = equationOfTime(dayOfYear);
    float corr   = (4.0f * (tzLon - longitude) + eot) / 60.0f;
    return localHour + corr;
}

static void solarPosition(int dayOfYear, float solarHour, float latitude,
                          float &alpha, float &gamma_s)
{
    float phi    = deg2rad(latitude);
    float delta  = deg2rad(23.45f * sin(deg2rad(360.0f * (284 + dayOfYear) / 365.0f)));
    float omega  = deg2rad(15.0f * (solarHour - 12));

    alpha = asin(sin(phi) * sin(delta) + cos(phi) * cos(delta) * cos(omega));
    gamma_s = atan2(cos(delta) * sin(omega),
                    cos(phi) * sin(delta) - sin(phi) * cos(delta) * cos(omega));
}

static float incidenceFactor(float alpha, float gamma_s, float tiltDeg, float azimDeg)
{
    float beta    = deg2rad(tiltDeg);
    float gamma_m = deg2rad(azimDeg);
    float ctheta  = sin(alpha) * cos(beta)
                  + cos(alpha) * sin(beta) * cos(gamma_s - gamma_m);
    return max(0.0f, ctheta);
}

/* ─── PV-Modell: Energie pro Stunde [kWh] ─────────────────────────── */
static float pvEnergyPerHour(float G_tilt, float T_amb)
{
    float P_mod_raw  = P_stc * (G_tilt / 1000.0f);
    float T_cell     = T_amb + (G_tilt / 800.0f) * (NOCT - 20.0f);
    float P_mod_temp = P_mod_raw * (1.0f + temp_coeff * (T_cell - 25.0f));
    float P_array    = N_mod * P_mod_temp * system_loss;
    float P_clipped  = min(P_array, inv_max_AC);
    return P_clipped / 1000.0f; // kWh
}

/* ─── Haupt-Routinen ──────────────────────────────────────────────── */
static void computeForecast(PROGNOSE &prognose);

bool wheater_fetch(PROGNOSE &prognose)
{
    LOG_INFO(TAG_WEATHER, "wheater_fetch BEGIN");

    // ── HTTP GET (lock-free, network I/O) ──
    int httpResponseCode = 0;
    String payload = util_GET_Request(PARAM, &httpResponseCode);

    if (httpResponseCode != 200)
    {
        LOG_ERROR(TAG_WEATHER, "Open-Meteo nicht erreichbar (HTTP %d)", httpResponseCode);
        prognose.valid = false;
        return false;
    }

    // ── JSON-Parsing ──
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        LOG_DEBUG(TAG_WEATHER, "JSON Decode Fehler: %s", error.c_str());
        prognose.valid = false;
        return false;
    }

    computeForecast(prognose);
    prognose.valid = true;

    LOG_INFO(TAG_WEATHER, "heute=%.0f Wh | morgen=%.0f Wh",
             prognose.forecastToday, prognose.forecastTomorow);
    return true;
}

/* ─── Aggregation: 48h PV-Daten → Energie(today/tomorrow) ─────────── */
static void computeForecast(PROGNOSE &prognose)
{
    JsonObject hourly = doc["hourly"];
    if (!hourly)
    {
        LOG_ERROR(TAG_WEATHER, "hourly nicht gefunden");
        prognose.valid = false;
        return;
    }

    JsonArray G_arr    = hourly["shortwave_radiation"];
    JsonArray cloud    = hourly["cloudcover"];
    JsonArray precip   = hourly["precipitation"];
    JsonArray sunsec   = hourly["sunshine_duration"];
    JsonArray Tamb     = hourly["temperature_2m"];
    JsonArray times    = hourly["time"];

    int n = time_GetDayOfYear();

    float energyToday    = 0; // Wh, risk-adjustiert
    float energyTomorrow = 0;

    // ── 48 Hours durchrechnen ──
    //    Stunden 00-23  → heute
    //    Stunden 24-47 → morgen
    for (int h = 0; h < 48; h++)
    {
        float G     = G_arr[h] | 0;
        float cc    = cloud[h] | 0;
        float pr    = precip[h] | 0;
        float sdur  = sunsec[h] | 0;
        float T_amb = Tamb[h] | 20;

        // ── Effektive Globalstrahlung (Sonnenschein-Dauer Skala) ──
        float G_eff = G * (sdur / 3600.0f);

        // ── Sonnen-Position + Einstrahl-Faktor auf das Modul ──
        float solarT = solarTime(h, n, LONGITUDE_f, tz_lon);
        float alpha, gamma_s;
        solarPosition(n, solarT, LATITUDE_f, alpha, gamma_s);
        float ctheta = incidenceFactor(alpha, gamma_s, tilt, azimut);
        float G_tilt = G_eff * ctheta;

        // ── PV-Energie pro Stunde [kWh] ──
        float E_h = pvEnergyPerHour(G_tilt, T_amb);

        // ── Unsicherheit (Wolken + Regen) → Risiko-Abzug ──
        float sigma = 0.9f * (cc / 100.0f) + (pr > 0.1f ? 0.3f : 0.0f);
        float E_adj = E_h * (1.0f - k_risk * sigma);

        if (h < 24)
            energyToday += E_adj;
        else
            energyTomorrow += E_adj;
    }

    // kWh → Wh
    prognose.forecastToday   = (int)(energyToday    * 1000.0f + 0.5f);
    prognose.forecastTomorow = (int)(energyTomorrow * 1000.0f + 0.5f);

    // ── Preheat-Bonus aus ratio ─────────────────────────────────────────
    float ratio = 0;
    if (prognose.forecastTomorow > 10) // Guard: keine Division durch 0
    {
        ratio = (float)prognose.forecastToday / (float)prognose.forecastTomorow;
    }

    int bonus = 0;
    if (ratio >= 2.5f)
        bonus = WEATHER_PREHEAT_MAX_BONUS;          // 8°C
    else if (ratio >= 1.5f)
        bonus = 5;                                  // 5°C
    else if (ratio >= 1.0f)
        bonus = 3;                                  // 3°C
    else
        bonus = 0;                                  // warten

    prognose.preheatBonus = bonus;
    prognose.validUntil   = time(nullptr) + WEATHER_DATA_MAX_AGE_SEC;

    LOG_INFO(TAG_WEATHER, "ratio=%.1f, bonus=%d°C, valid=%lu",
             ratio, bonus, (unsigned long)prognose.validUntil);
}

#endif
