#pragma once
#ifdef WEATHER_API
#define LATITUDE "48.24" // breitengrad
#define LATITUDE_f 48.24 // breitengrad
#define LONGITUDE "13.3208"
#define LONGITUDE_f 13.3208


#define FORCAST_DAYS_STRING "1"
#define FORCAST_DAYS 3
#define HOURS_PER_DAY 24
#define TEMPERATURE_SIZE FORCAST_DAYS *HOURS_PER_DAY
#define SUNDAY_LIGHT_SIZE FORCAST_DAYS *HOURS_PER_DAY
#define DAILY_VALUES_SIZE 3

/* typedef struct
{

    int sunrise[FORCAST_DAYS]; // 3
    int sunset[FORCAST_DAYS];  //3

} WHEATER_DATA;
 */

typedef struct _PROGNOSE
{
    bool loadImmediately; // lade heute
    bool loadDuringDay;   // lade heute
    int bestHour;         // beste Stunde heute
    bool loadTomorow;     // lade morgen
    int bestHourTomorow;  // beste Stunde morgen

} PROGNOSE;

bool wheater_fetch(PROGNOSE &prognose);

#endif