

#include "weather.h"
#include "utils.h"
#include "curTime.h"

#include <Arduino_JSON.h>

#ifdef WEATHER_API

#define HOST_NAME WEATHER_API
#define PATH_NAME_FORECAST "/v1/forecast"

/* #define PARAM HOST_NAME PATH_NAME_FORECAST "?latitude=" LATITUDE "&longitude=" LONGITUDE "&hourly=cloudcover,shortwave_radiation,precipitation,sunshine_duration,temperature_2m&timezone=Europe%2FVienna&forecast_days=" FORCAST_DAYS_STRING */

const char *PARAM = "https://api.open-meteo.com/v1/forecast?latitude=48.24&longitude=13.3208&hourly=cloudcover,shortwave_radiation,precipitation,sunshine_duration,temperature_2m&timezone=Europe%2FBerlin&forecast_days=1";

#define JSON_ARRAY_SIZE 8500

/* "https://api.open-meteo.com/v1/forecast?"
    "latitude=48.21&longitude=16.37"
    "&hourly=cloudcover,shortwave_radiation,precipitation,sunshine_duration,temperature_2m"
    "&timezone=Europe%2FBerlin&forecast_days=1";
 */
/*
Nutzer-Parameter (einmalig zu konfigurieren)

Standortdaten

latitude (Breitengrad)

longitude (Längengrad)

tz_lon (Standardmeridian der Zeitzone, z. B. 15°E für MEZ, 0° für UTC)

PV-Anlage

--- PV-Anlage (Nutzer-Parameter) ---
const float P_stc       = 380.0;    // W pro Modul bei STC
const int   N_mod       = 6;        // Anzahl Module
const float NOCT        = 45.0;     // °C, nominal operating cell temp
const float temp_coeff  = -0.0035;  // -0.35%/K = -0.0035
const float system_loss = 0.88;     // Gesamtverluste (Soiling, Kabel, MPPT etc.)
const float inv_max_AC  = 2000.0;   // Wechselrichter AC-Nennleistung [W]
const float tilt        = 30.0;     // Modulneigung [°]
const float azimut      = 0.0;      // Azimut (0 = Süd, -90 = Ost, +90 = West)

Score-Gewichte

w_E – Gewicht Energiebeitrag (z. B. 1.0)

w_σ – Gewicht Unsicherheit (z. B. 0.5)

w_T – Gewicht Mittagsbonus (z. B. 0.3)


*/

// static WHEATER_DATA wheaterData;

// PV
const float area_m2 = 2.0;
const float eff = 0.18;
// --- PV-Anlage (Nutzer-Parameter) ---
const float P_stc = 405.0;        // W pro Modul bei STC
const int N_mod = 36;             // Anzahl Module
const float NOCT = 45.0;          // °C, nominal operating cell temp
const float temp_coeff = -0.0035; // -0.35%/K = -0.0035
const float system_loss = 0.88;   // Gesamtverluste (Soiling, Kabel, MPPT etc.)
const float inv_max_AC = 10000.0; // Wechselrichter AC-Nennleistung [W]
const float tilt = 30.0;          // Modulneigung [°]
const float azimut = 159.0;       // Azimut (0 = Süd, -90 = Ost, +90 = West); südwest mit 159
const float tz_lon = 15.0;        // Standardmeridian (z.B. 15°E für MEZ)
// Risikoparameter
const float k_risk = 0.5;

// Score-Gewichte
const float w_E = 1.0;     // Gewicht Energiebeitrag (z. B. 1.0)
const float w_sigma = 0.5; // Gewicht Unsicherheit (z. B. 0.5)
const float w_T = 0.3;     // Gewicht Mittagsbonus (z. B. 0.3)
const float sigma_T = 2.0; // Breite der Mittagsglocke

static void wheater_ladestrategie(DynamicJsonDocument &doc, String &json_array, PROGNOSE &prognose);

//-- -Hilfsfunktionen-- -
float deg2rad(float d) { return d * PI / 180.0; }
float rad2deg(float r) { return r * 180.0 / PI; }

static DynamicJsonDocument doc(JSON_ARRAY_SIZE);

bool wheater_fetch(PROGNOSE &prognose)
{

    int httpResponseCode = 0;

    LOG_INFO("wheater::wheater_fetch BEGIN")

    String json_array = util_GET_Request(PARAM, &httpResponseCode);
    // LOG_INFO("URL: %s", PARAM);

    if (httpResponseCode != 200)
    {
        LOG_ERROR("URL: %s is not available, ResponseCode: %d", PARAM, httpResponseCode);
        LOG_INFO("wheater_fetch eXIT false");
        return false;
    }
    Serial.println(json_array);
    // JSON Puffer (ca. 20k reicht für 2 Tage Daten)

    DeserializationError error = deserializeJson(doc, json_array);

    if (error == DeserializationError::Ok)
    {
        wheater_ladestrategie(doc, json_array, prognose);
    }
    else
    {
        LOG_DEBUG("wheater_fetch::JSON Fehler: %s", error.c_str());
        LOG_INFO("wheater_fetch eXIT false");

        return false;
    }
    LOG_INFO("wheater_fetch eXIT true");
    return true;
}

float equationOfTime(int n)
{
    float B = deg2rad((360.0 / 365.0) * (n - 81));
    return 9.87 * sin(2 * B) - 7.53 * cos(B) - 1.5 * sin(B);
}

float solarTime(float localHour, int dayOfYear, float longitude, float tz_lon)
{
    float eot = equationOfTime(dayOfYear);
    float corr = (4 * (tz_lon - longitude) + eot) / 60.0;
    return localHour + corr;
}

void solarPosition(int dayOfYear, float solarHour, float latitude, float &alpha, float &gamma_s)
{
    float phi = deg2rad(latitude);
    float delta = deg2rad(23.45 * sin(deg2rad(360.0 * (284 + dayOfYear) / 365.0)));
    float omega = deg2rad(15.0 * (solarHour - 12));

    alpha = asin(sin(phi) * sin(delta) + cos(phi) * cos(delta) * cos(omega));
    gamma_s = atan2(cos(delta) * sin(omega),
                    cos(phi) * sin(delta) - sin(phi) * cos(delta) * cos(omega));
}

float incidenceFactor(float alpha, float gamma_s, float tilt, float azimut)
{
    float beta = deg2rad(tilt);
    float gamma_m = deg2rad(azimut);
    float ctheta = sin(alpha) * cos(beta) + cos(alpha) * sin(beta) * cos(gamma_s - gamma_m);
    return max(0.0f, ctheta);
}

float middayBonus(int h)
{
    float diff = h - 12;
    return exp(-(diff * diff) / (2 * sigma_T * sigma_T));
}

float computeScore(float E_adj, float Emax, float sigma, int h)
{
    float Bh = middayBonus(h);
    float sE = (Emax > 0) ? (E_adj / Emax) : 0.0;
    return w_E * sE - w_sigma * sigma + w_T * Bh;
}

// --- PV-Modell mit P_stc, Temperatur, Clipping ---
float pvEnergyPerHour(float G_tilt, float T_amb)
{
    // 1) Roh-Leistung pro Modul
    float P_mod_raw = P_stc * (G_tilt / 1000.0f);

    // 2) Zelltemperatur (°C)
    float T_cell = T_amb + (G_tilt / 800.0f) * (NOCT - 20.0f);

    // 3) Temperaturkorrektur
    float P_mod_temp = P_mod_raw * (1.0f + temp_coeff * (T_cell - 25.0f));

    // 4) Array-DC
    float P_array_DC = N_mod * P_mod_temp;

    // 5) Systemverluste
    float P_usable = P_array_DC * system_loss;

    // 6) Clipping am Inverter
    float P_final_AC = min(P_usable, inv_max_AC);

    // 7) Energie pro Stunde [kWh]
    return P_final_AC / 1000.0f;
}

static void wheater_ladestrategie(DynamicJsonDocument &doc, String &payload, PROGNOSE &prognose)
{
    if (deserializeJson(doc, payload) == DeserializationError::Ok)
    {
        JsonObject hourly = doc["hourly"];
        JsonArray G_arr = hourly["shortwave_radiation"];
        JsonArray cloud = hourly["cloudcover"];
        JsonArray precip = hourly["precipitation"];
        JsonArray sunsec = hourly["sunshine_duration"];
        JsonArray Tamb = hourly["temperature_2m"];

        int n = time_GetDayOfYear();

        float E_adj_hour[24] = {0};
        float sigma_hour[24] = {0};
        float Emax = 0;

        for (int h = 0; h < 24; h++)
        {
            float G = G_arr[h] | 0;
            float cc = cloud[h] | 0;
            float pr = precip[h] | 0;
            float sdur = sunsec[h] | 0;
            float T_amb = Tamb[h] | 20; // falls fehlt, default 20°C

            float G_eff = G * (sdur / 3600.0f);

            float solarT = solarTime(h, n, LONGITUDE_f, tz_lon);
            float alpha, gamma_s;
            solarPosition(n, solarT, LATITUDE_f, alpha, gamma_s);

            float ctheta = incidenceFactor(alpha, gamma_s, tilt, azimut);
            float G_tilt = G_eff * ctheta;

            float E_h = pvEnergyPerHour(G_tilt, T_amb);

            float sigma = 0.9 * (cc / 100.0) + (pr > 0.1 ? 0.3 : 0.0);
            float E_adj = E_h * (1 - k_risk * sigma);

            E_adj_hour[h] = E_adj;
            sigma_hour[h] = sigma;
            if (E_adj > Emax)
                Emax = E_adj;

            /* Serial.printf("Stunde %02d: G=%.1f, cc=%.0f%%, rain=%.2f, T=%.1f°C, "
                          "E_h=%.3f kWh (adj=%.3f)\n",
                          h, G, cc, pr, T_amb, E_h, E_adj); */
        }

        Serial.println("\n--- Score-Bewertung ---");
        for (int h = 0; h < 24; h++)
        {
            float Sh = computeScore(E_adj_hour[h], Emax, sigma_hour[h], h);
            Serial.printf("Stunde %02d: E_adj=%.3f kWh, σ=%.2f, Score=%.2f",
                          h, E_adj_hour[h], sigma_hour[h], Sh);
            if (Sh > 0.5)
                Serial.print(" -> Laden empfohlen!");
            Serial.println();
        }
    }
    else
    {
        //Serial.println("JSON Fehler in wheater_ladestrategie");
        LOG_ERROR("wheather:wheater_ladestrategie JSON Fehler");
    }
}

#endif