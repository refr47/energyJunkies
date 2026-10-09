#pragma once
#include <ctime>
#ifdef WEATHER_API
#define LATITUDE "48.24"          // Breitengrad
#define LATITUDE_f 48.24
#define LONGITUDE "13.3208"
#define LONGITUDE_f 13.3208

#define FORCAST_DAYS_STRING "2"   // heute + morgen
#define FORCAST_DAYS 2
#define HOURS_PER_DAY 24
#define TEMPERATURE_SIZE FORCAST_DAYS * HOURS_PER_DAY
#define SUNDAY_LIGHT_SIZE FORCAST_DAYS * HOURS_PER_DAY
#define DAILY_VALUES_SIZE 2

#define WEATHER_PREHEAT_MAX_BONUS  8      // maximale °C Bonus (safety cap)
#define WEATHER_PREHEAT_MAX_SAFE  65     // absolutes Temp-Max auch mit Bonus
#define WEATHER_DATA_MAX_AGE_SEC  (6*3600) // Wetter-Daten älter? → Bonus=0
#define WEATHER_FETCH_INTERVAL_MS (6*3600*1000) // alle 6h neu abfragen

/*
 * PROGNOSE – Output der Wetter-Wertung.
 *
 * preheatBonus  : in °C, wie weit über setupData.tempMaxAllowedInGrad
 *                 der Boiler heute geheizt werden soll. 0 = kein Bonus.
 * validUntil    : Unix-Zeitstempel, wann die Daten verfallen.
 * forecastToday : PV-Energie heute   [Wh], adjustiert (Wolken/Risiko)
 * forecastTomorow: PV-Energie morgen [Wh], adjustiert
 */
typedef struct _PROGNOSE
{
    int  preheatBonus;                // °C Bonus (0..WEATHER_PREHEAT_MAX_BONUS)
    int  forecastToday;               // Wh heute (adjusted)
    int  forecastTomorow;             // Wh morgen (adjusted)
    bool valid;                       // true wenn Daten aktuell
    time_t validUntil;                // Unix-TS wann Daten verfallen
} PROGNOSE;

bool wheater_fetch(PROGNOSE &prognose);

#endif
