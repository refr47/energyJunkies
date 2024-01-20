#include "froniusSolarAPI.h"
#ifdef FRONIUS_API
#include "utils.h"
#include <Arduino_JSON.h>
#include "defines.h"
#include "debugConsole.h"

#define PATH_NAME_FORECAST "/status/powerflow"

static String uRL = "";

bool soloar_init(Setup &setup)
{
    char buf[50];
    sprintf(buf, "http://%s%s", setup.ipInverterAsString.c_str(), PATH_NAME_FORECAST);
    uRL = buf;
    setup.externerSpeicher = false;
    String json_array = util_GET_Request(uRL.c_str());
    JSONVar my_obj = JSON.parse(json_array);
    if (JSON.typeof(my_obj) == "undefined")
    {
        DBG("Parsing input failed!");
        return false;
    }
    if (isdigit(my_obj["site"]["P_Akku"]))
    {
        setup.externerSpeicher = true;
        DBG("solar_init - akku vorhanden!");
    }
    return true;
}

bool solar_get_powerflow(FRONIUS_SOLAR_POWERFLOW &container)
{

    DBGf("solar_get_powerflow() - uRL: %s", uRL.c_str());
    String json_array = util_GET_Request(uRL.c_str());
    // DBGf("solar_get_powerFlow(): %s", json_array);
    JSONVar my_obj = JSON.parse(json_array);
    if (JSON.typeof(my_obj) == "undefined")
    {
        DBG("Parsing input failed!");
        return false;
    }

    container.p_akku = my_obj["site"]["P_Akku"];
    container.p_grid = my_obj["site"]["P_Grid"];
    container.p_load = my_obj["site"]["P_Load"];
    container.p_pv = my_obj["site"]["P_PV"];
    container.rel_Autonomy = my_obj["site"]["rel_Autonomy"];
    container.rel_SelfConsumption = my_obj["site"]["rel_SelfConsumption"];

#ifdef MODBUS_VERBOSE
    DBGf("Fronius API: ");
    DBGf("AKKU: %f", container.p_akku);
    DBGf("Grid: %f", container.p_grid);
    DBGf("Load: %f", container.p_load);
    DBGf("PV: %f", container.p_pv);
    DBGf("SelfConsumption: %f", container.rel_SelfConsumption);
    DBGf("rel_Autonomy: %f", container.rel_Autonomy);

#endif
    return true;
}

#endif