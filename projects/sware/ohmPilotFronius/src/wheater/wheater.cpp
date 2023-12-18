#include <HTTPClient.h>
#include "wheater.h"
#include <Arduino_JSON.h>

#define HOST_NAME "https://api.open-meteo.com"
#define PATH_NAME_FORECAST "/v1/forecast"

#define PARAM HOST_NAME PATH_NAME_FORECAST "?latitude=" LATITUDE "&longitude=" LONGITUDE "&hourly=temperature_2m"

String GET_Request(const char *server)
{
    HTTPClient http;
    http.begin(server);
    int httpResponseCode = http.GET();

    String payload = "{}";

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        payload = http.getString();
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }
    http.end();

    return payload;
}

void wheater_getForecast()
{

    String json_array = GET_Request(PARAM);
    Serial.println(json_array);
    JSONVar my_obj = JSON.parse(json_array);
    Serial.println(my_obj);
}