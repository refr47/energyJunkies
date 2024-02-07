#include "froniusSolarAPI.h"
#ifdef FRONIUS_API
#include "utils.h"
#include <Arduino_JSON.h>
#include "defines.h"
#include "debugConsole.h"

#define PATH_NAME_FORECAST "/status/powerflow"

static String uRL = "";

bool soloar_init(Setup &setup, bool *akku)
{
    char buf[50];
    int httpResponseCode=0;
    sprintf(buf, "http://%s%s", setup.ipInverterAsString.c_str(), PATH_NAME_FORECAST);
    uRL = buf;
    DBGf("solar_init() for %s", buf);
    setup.externerSpeicher = false;
    *akku = false;
    String json_array = util_GET_Request(uRL.c_str(),&httpResponseCode);
    if (httpResponseCode != 200) {
        DBGf("solar_init:: Fronius API nicht erreichbar - kein Fronius Inverter?");
        return false;
    }
    JSONVar my_obj = JSON.parse(json_array);
    if (JSON.typeof(my_obj) == "undefined")
    {
        DBG("Parsing input failed!");
        return false;
    }
    if (JSON.typeof(my_obj["site"]["P_Akku"]) == "undefined")
    {
        DBG("solar_init Akku is not available!!");
    }
    else
    {
        setup.externerSpeicher = true;
        *akku = true;
        DBGf("solar_init - akku  vorhanden!");
    }
    DBGf("solar_init() EXIT with akku: %d", setup.externerSpeicher);
    return true;
}

bool solar_get_powerflow(FRONIUS_SOLAR_POWERFLOW &container)
{

    DBGf("solar_get_powerflow() - uRL: %s", uRL.c_str());
    int htppResponse=0;
    String json_array = util_GET_Request(uRL.c_str(),&htppResponse);
    if (htppResponse !=200) {
        DBGf("solar_get_powerflow:: ResponsCode != 200");
        return false;
    }
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

/* #ifdef MODBUS_VERBOSE
    DBGf("Fronius API: ");
    DBGf("AKKU: %f", container.p_akku);
    DBGf("Grid: %f", container.p_grid);
    DBGf("Load: %f", container.p_load);
    DBGf("PV: %f", container.p_pv);
    DBGf("SelfConsumption: %f", container.rel_SelfConsumption);
    DBGf("rel_Autonomy: %f", container.rel_Autonomy);

#endif */
    return true;
}

#endif