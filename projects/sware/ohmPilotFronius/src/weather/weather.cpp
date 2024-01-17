

#include "weather.h"
#include "utils.h"
#include <Arduino_JSON.h>

#ifdef WEATHER_API

#define HOST_NAME "https://api.open-meteo.com" 
#define PATH_NAME_FORECAST "/v1/forecast"

#define PARAM HOST_NAME PATH_NAME_FORECAST "?latitude=" LATITUDE "&longitude=" LONGITUDE "&hourly=temperature_2m"

void wheater_getForecast()
{

    String json_array = util_GET_Request(PARAM);
    Serial.println(json_array);
    JSONVar my_obj = JSON.parse(json_array);
    Serial.println(my_obj);
}

#endif