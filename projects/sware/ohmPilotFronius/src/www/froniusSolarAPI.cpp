#include "froniusSolarAPI.h"
#ifdef FRONIUS_API
#include "utils.h"
#include <Arduino_JSON.h>
#include "defines.h"
#include "debugConsole.h"

#define PATH_NAME_FORECAST "/status/powerflow"

static String uRL = "";

void soloar_init(Setup &setup)
{
    char buf[50];
    sprintf(buf, "http://%s%s", setup.ipInverterAsString.c_str(), PATH_NAME_FORECAST);
    uRL = buf;
}

bool solar_get_powerflow(FRONIUS_SOLAR_POWERFLOW &container)
{

    DBGf("solar_get_powerflow() - uRL: %s", uRL.c_str());
    String json_array = util_GET_Request(uRL.c_str());
    DBGf("solar_get_powerFlow(): %s", json_array);
    JSONVar my_obj = JSON.parse(json_array);
    double akku = my_obj["site"]["P_Akku"];
    DBGf("akku: %f", akku);
    return true;
}

#endif