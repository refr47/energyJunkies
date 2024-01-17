#include "froniusSolarAPI.h"
#ifdef FRONIUS_API
#include "utils.h"
#include <Arduino_JSON.h>
#include "defines.h"
#include "debugConsole.h"

#define PATH_NAME_FORECAST "/status/powerflow"

static String URL = "";

void soloar_init(Setup &setup)
{
    char buf[50];
    sprintf(buf, "http:// %s%s", setup.ipInverterAsString.c_str(), PATH_NAME_FORECAST);
    URL = buf;
}

bool solar_get_powerflow(FRONIUS_SOLAR_POWERFLOW &container)
{

    String json_array = util_GET_Request(URL.c_str());
    DBGf("solar_get_powerFlow(): %s", json_array);
    JSONVar my_obj = JSON.parse(json_array);
    JSONVar keys = my_obj.keys();
    for (int i = 0; i < keys.length(); i++)
    {
        JSONVar value = my_obj[keys[i]];
        Serial.print(keys[i]);
        Serial.print(" = ");
        Serial.println(value);
      
    }
}

#endif