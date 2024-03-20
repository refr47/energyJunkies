

#include "weather.h"
#include "utils.h"
#include <Arduino_JSON.h>
#include <time.h>

#ifdef WEATHER_API

#define HOST_NAME WEATHER_API
#define PATH_NAME_FORECAST "/v1/forecast"

#define PARAM HOST_NAME PATH_NAME_FORECAST "?latitude=" LATITUDE "&longitude=" LONGITUDE "&hourly=temperature_2m,sunshine_duration&daily=sunrise,sunset,sunshine_duration,uv_index_max&timezone=Europe%2FVienna&forecast_days=" FORCAST_DAYS_STRING

static WHEATER_DATA wheater;

static void
interpretWeather(JSONVar &data);
static time_t wheater_parseDateTime(const char *date_string);
static void wheater_timestampToDate(time_t timestamp, char *buffer);

void wheater_getForecast()
{

    int httpResponseCode = 0;
    String json_array = util_GET_Request(PARAM, &httpResponseCode);
    // Serial.println(json_array);
    JSONVar my_obj = JSON.parse(json_array);
    // Serial.println(my_obj);
    DBGf("wheater_getForecast URL: %s", PARAM);
    interpretWeather(my_obj);
}

/*
{
time: [
"2024-03-18",
"2024-03-19",
"2024-03-20"
],
sunrise: [
"2024-03-18T06:12",
"2024-03-19T06:09",
"2024-03-20T06:07"
],
sunset: [
"2024-03-18T18:16",
"2024-03-19T18:18",
"2024-03-20T18:19"
],
daylight_duration: [
43435.11,
43683.6,
43932.5
],
sunshine_duration: [
36203.57,
34819.75,
12636.17
],
uv_index_max: [
3.4,
3.7,
3.15
],

*/
// char buffer[20]; // Adjust buffer size as needed
static void wheater_timestampToDate(time_t timestamp, char *buffer)
{
    struct tm *tm_info;

    // Convert timestamp to broken-down time (local time)
    tm_info = localtime(&timestamp);

    // Format the time into a string
    // char buffer[20]; // Adjust buffer size as needed
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", tm_info);

    // Print the formatted date string
}

static time_t wheater_parseDateTime(const char *date_string)
{
    struct tm tm_time = {0};

    // Parse the date string
    if (strptime(date_string, "%Y-%m-%dT%H:%M", &tm_time) == NULL)
    {
        DBGf("wheater_parseDateTime::Error: Unable to parse date string: %s\n", date_string);
        return 0;
    }

    // Convert the parsed time to a timestamp
    time_t timestamp = mktime(&tm_time);
    if (timestamp == -1)
    {
        DBGf("wheater_parseDateTime::Unable to convert time to timestamp");
        return 0;
    }

    return timestamp;
}

static void interpretWeather(JSONVar &data)
{

    JSONVar temperatureArray = data["hourly"]["temperature_2m"];
    Serial.print("interpretWeather ");
    Serial.println(JSON.typeof(temperatureArray));
    if (JSON.typeof(temperatureArray) == "array") // temperatureArray is an array
    {
        for (int i = 0; i < temperatureArray.length(); i++)
        {
            /*  Serial.print("time: ");
             Serial.println(temperatureArray[i]); */
            wheater.temperature[i] = temperatureArray[i];
        }
    }
    JSONVar sunnArray = data["hourly"]["sunshine_duration"];
    if (JSON.typeof(sunnArray) == "array") // temperatureArray is an array
    {
        for (int i = 0; i < sunnArray.length(); i++)
        {
            /*  Serial.print("sunDaylight: ");
             Serial.println(sunnArray[i]); */
            wheater.daylight[i] = sunnArray[i];
        }
    }
    JSONVar sunRise = data["daily"]["sunrise"];
    if (JSON.typeof(sunRise) == "array") // temperatureArray is an array
    {
        for (int i = 0; i < sunRise.length(); i++)
        {
            /* Serial.print("sunRise: ");
            Serial.println(sunRise[i]); */
            wheater.sunrise[i] = wheater_parseDateTime(sunRise[i]);
        }
    }
    JSONVar sunSet = data["daily"]["sunset"];
    Serial.println("Type of uvIndex: " + JSON.typeof(sunSet[0]));
    if (JSON.typeof(sunSet) == "array") // temperatureArray is an array
    {
        for (int i = 0; i < sunSet.length(); i++)
        {
            /*   Serial.print("sunSet: ");
              Serial.println(sunSet[i]); */
            wheater.sunset[i] = wheater_parseDateTime(sunSet[i]);
        }
    }
    /*  JSONVar sunDur = data["daily"]["sunshine_duration"];
     if (JSON.typeof(sunDur) == "array") // temperatureArray is an array
     {

         for (int i = 0; i < sunDur.length(); i++)
         {

             Serial.println(sunDur[i]);
         }
     }  */
    // uv_index_max
    JSONVar uvIndex = data["daily"]["uv_index_max"];
    Serial.println("Type of uvIndex: " + JSON.typeof(uvIndex[0]));
    if (JSON.typeof(uvIndex) == "array") // temperatureArray is an array
    {
        for (int i = 0; i < uvIndex.length(); i++)
        {
            /*   Serial.print("uvIndex: ");
              Serial.println(uvIndex[i]); */
            wheater.uvIndex[i] = uvIndex[i];
        }
    }
}

void wheater_print()
{
    DBGf("wheater - print data");
    DBG("Temperatur");
    for (int j = 0; j < TEMPERATURE_SIZE; j++)
    {
        if (j != 0 && j % 24 == 0)
        {
            DBG("Next day");
        }
        DBGf("%d:: %f", j % 24, wheater.temperature[j]);
    }
    DBG("Sonnendauer");
    for (int j = 0; j < SUNDAY_LIGHT_SIZE; j++)
    {
        if (j != 0 && j % 24 == 0)
        {
            DBG("Next day");
        }
        DBGf("%d:: %f", j % 24, wheater.daylight[j]);
    }
    DBG("Sonnenaufgang");
    for (int j = 0; j < DAILY_VALUES_SIZE; j++)
    {
        DBGf("%d:: %f", j, wheater.sunrise[j]);
    }
    DBG("Sonnenuntergang");
    for (int j = 0; j < DAILY_VALUES_SIZE; j++)
    {
        DBGf("%d:: %f", j, wheater.sunset[j]);
    }
    DBG("UV-Index");
    for (int j = 0; j < DAILY_VALUES_SIZE; j++)
    {
        DBGf("%d:: %f", j, wheater.uvIndex[j]);
    }
}
#endif

#ifdef EX

{
latitude:
    52.52,
        longitude : 13.419998,
        generationtime_ms : 0.1289844512939453,
        utc_offset_seconds : 3600,
        timezone : "Europe/Berlin",
                   timezone_abbreviation : "CET",
                                           elevation : 38,
                                           hourly_units : {
                                               time : "iso8601",
                                               temperature_2m : "°C",
                                               precipitation_probability : "%",
                                               precipitation : "mm",
                                               cloud_cover : "%",
                                               cloud_cover_low : "%",
                                               cloud_cover_mid : "%",
                                               cloud_cover_high : "%",
                                               sunshine_duration : "s"
                                           },
                                                          hourly : {
                                                              time : [
                                                                  "2024-03-18T00:00",
                                                                  "2024-03-18T01:00",
                                                                  "2024-03-18T02:00",
                                                                  "2024-03-18T03:00",
                                                                  "2024-03-18T04:00",
                                                                  "2024-03-18T05:00",
                                                                  "2024-03-18T06:00",
                                                                  "2024-03-18T07:00",
                                                                  "2024-03-18T08:00",
                                                                  "2024-03-18T09:00",
                                                                  "2024-03-18T10:00",
                                                                  "2024-03-18T11:00",
                                                                  "2024-03-18T12:00",
                                                                  "2024-03-18T13:00",
                                                                  "2024-03-18T14:00",
                                                                  "2024-03-18T15:00",
                                                                  "2024-03-18T16:00",
                                                                  "2024-03-18T17:00",
                                                                  "2024-03-18T18:00",
                                                                  "2024-03-18T19:00",
                                                                  "2024-03-18T20:00",
                                                                  "2024-03-18T21:00",
                                                                  "2024-03-18T22:00",
                                                                  "2024-03-18T23:00"
                                                              ],
                                                              temperature_2m : [
                                                                  0.8,
                                                                  0.2,
                                                                  -0.2,
                                                                  -0.4,
                                                                  -1.2,
                                                                  -1.4,
                                                                  -1.5,
                                                                  -1.6,
                                                                  -1.1,
                                                                  0.2,
                                                                  2.8,
                                                                  4.2,
                                                                  5.4,
                                                                  6.2,
                                                                  6.7,
                                                                  6.9,
                                                                  6.7,
                                                                  6,
                                                                  5.2,
                                                                  3.8,
                                                                  2.8,
                                                                  2,
                                                                  1.3,
                                                                  0.8
                                                              ],
                                                              precipitation_probability : [
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0
                                                              ],
                                                              precipitation : [
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0
                                                              ],
                                                              cloud_cover : [
                                                                  30,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  100,
                                                                  100,
                                                                  100,
                                                                  97,
                                                                  29,
                                                                  100,
                                                                  100,
                                                                  81,
                                                                  100,
                                                                  100,
                                                                  97,
                                                                  100,
                                                                  100,
                                                                  95,
                                                                  58,
                                                                  73,
                                                                  3,
                                                                  60,
                                                                  0,
                                                                  0
                                                              ],
                                                              cloud_cover_low : [
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  6,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0
                                                              ],
                                                              cloud_cover_mid : [
                                                                  30,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  52,
                                                                  7,
                                                                  100,
                                                                  86,
                                                                  100,
                                                                  100,
                                                                  95,
                                                                  58,
                                                                  73,
                                                                  0,
                                                                  60,
                                                                  0,
                                                                  0
                                                              ],
                                                              cloud_cover_high : [
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  100,
                                                                  100,
                                                                  100,
                                                                  97,
                                                                  29,
                                                                  100,
                                                                  100,
                                                                  71,
                                                                  100,
                                                                  100,
                                                                  90,
                                                                  100,
                                                                  100,
                                                                  5,
                                                                  0,
                                                                  0,
                                                                  3,
                                                                  0,
                                                                  0,
                                                                  0
                                                              ],
                                                              sunshine_duration : [
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  0,
                                                                  3600,
                                                                  3600,
                                                                  3600,
                                                                  3600,
                                                                  sunshine_duration:3600,
                                                                                    3600,
                                                                                    3600,
                                                                                    3600,
                                                                                    3600,
                                                                                    2498.15,
                                                                                    0,
                                                                                    0,
                                                                  sunshine_duration:0,
                                                                                    0,
                                                                                    0,
                                                                                    0
                                                              ]
                                                          },
                                                                   daily_units : {
                                                                       time : "iso8601",
                                                                       sunrise : "iso8601",
                                                                       sunset : "iso8601",
                                                                       daylight_duration : "s",
                                                                       sunshine_duration : "s",
                                                                       uv_index_max : "",
                                                                       precipitation_sum : "mm",
                                                                       rain_sum : "mm",
                                                                       showers_sum : "mm",
                                                                       snowfall_sum : "cm",
                                                                       precipitation_hours : "h",
                                                                       precipitation_probability_max : "%"
                                                                   },
                                                                                 daily:
    {
    time:
        ["2024-03-18"],
            sunrise : ["2024-03-18T06:12"],
                      sunset : ["2024-03-18T18:16"],
                               daylight_duration : [43435.11],
                                                   sunshine_duration : [34898.16],
                                                                       uv_index_max : [3.4],
                                                                                      precipitation_sum : [0],
                                                                                                          rain_sum : [0],
                                                                                                                     showers_sum : [0],
                                                                                                                                   snowfall_sum : [0],
                                                                                                                                                  precipitation_hours : [0],
                                                                                                                                                                        precipitation_probability_max : [0]
    }
}

#endif