#include "froniusSolarAPI.h"
#ifdef FRONIUS_API
#include "utils.h"
#include <Arduino_JSON.h>
#include "defines.h"
#include "debugConsole.h"

#define PATH_NAME_FORECAST "/status/powerflow"

static String uRL = "";
  
static bool solar_valid = false;
static bool p_solarValid = &solar_valid;

static void mapJsonValuesFronius(HTTP_REST_TARGET_t *target, char *jsonStart, WEBSOCK_DATA &webSockData);

KEY_VALUE_MAP_t froniusKeyValueMap[FRONIUS_VALUE_COUNT] = {
    {"site.P_Akku", 0},
    {"site.P_Grid", 1},
    {"site.P_Load", 2},
    {"site.P_PV", 3},
    {"site.rel_Autonomy", 4},
    {"site.rel_SelfConsumption", 5},

}; 

bool soloar_init(WEBSOCK_DATA &webSockData, bool *akku)
{
#ifdef CCC
    char buf[50];
    int httpResponseCode = 0;
    DBGf("solar_init BEGIN ");
    p_solarValid = &webSockData.states.froniusAPI;

    sprintf(buf, "http://%s%s", webSockData.setup.inverterAsString, PATH_NAME_FORECAST);
    uRL = buf;
    DBGf("solar_init() for %s", buf);
    webSockData.setup.externerSpeicher = false;
    *akku = false;

    String json_array = util_GET_Request(uRL.c_str(), &httpResponseCode);
    if (httpResponseCode != 200)
    {
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
        webSockData.setup.externerSpeicher = true;
        *akku = true;
        DBGf("solar_init - akku  vorhanden!");
    }
#endif

    DBGf("froniusSolarAPI::soloar_init - fronius HOST: %s", webSockData.setupData.inverterAsString);
 
    if (utils_sock_initRestTargets(webSockData.setupData.inverterAsString, FRONIUS_SOLAR_API_INDEX))
    {
        DBGf("solar_init() - init REST API success");
        webSockData.setupData.externerSpeicher = true;
        DBGf("solar_init() EXIT with akku: %d and retVal: TRUE", webSockData.setupData.externerSpeicher);
        *akku = true;
        return true;
    }
    webSockData.setupData.externerSpeicher = false;
    *akku = false;
    DBGf("solar_init() EXIT with akku: %d and retVal: FALSE", webSockData.setupData.externerSpeicher);
    return false;
}

bool solar_get_powerflow(WEBSOCK_DATA &webSockData)
{

    DBGf("solar_get_powerflow() - uRL: %s", uRL.c_str());
    return utils_sock_readRestTarget(webSockData, AMIS_READER_INDEX, mapJsonValuesFronius);
}

#ifdef CCC
int htppResponse = 0;
String json_array = util_GET_Request(uRL.c_str(), &htppResponse);
if (htppResponse != 200)
{
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

static void mapJsonValuesFronius(HTTP_REST_TARGET_t *target, char *jsonStart, WEBSOCK_DATA &webSockData)
{
    StaticJsonDocument<512> jsonBuffer;
    DBGf("froniusSolarAPI::mapJsonValuesFronius ENTER");

    // DBGf("mapJsonValues   ENTER  %s", jsonStart);

    deserializeJson(jsonBuffer, jsonStart);
    // DBGf("mapJsonValues %s", jsonStart);

    for (int i = 0; i < target->valueCount; i++)
    {
        /*  DBGf("map, key: %s", target->mapping[i].key);
         DBGf("map, jsonObj: %d", jsonBuffer[target->mapping[i].key]); */
        switch (i)
        {
        case 0:
            webSockData.fronius_SOLAR_POWERFLOW.p_akku = jsonBuffer[target->mapping[i].key];
            break;
        case 1:
            webSockData.fronius_SOLAR_POWERFLOW.p_grid = jsonBuffer[target->mapping[i].key];
            break;
        case 2:
            webSockData.fronius_SOLAR_POWERFLOW.p_load = jsonBuffer[target->mapping[i].key];
            break;
        case 3:
            webSockData.fronius_SOLAR_POWERFLOW.p_pv = jsonBuffer[target->mapping[i].key];
            break;
        case 4:
            webSockData.fronius_SOLAR_POWERFLOW.rel_Autonomy = jsonBuffer[target->mapping[i].key];
            break;
        case 5:
            webSockData.fronius_SOLAR_POWERFLOW.rel_SelfConsumption = jsonBuffer[target->mapping[i].key];
            break;
        default:
            DBGf("solar_get_powerflow::mapJsonValues() no mapping found for index: %d", i);
        }
        DBGf("froniusSolarAPI::mapJsonValuesFronius ExIT");
    }
}

#endif
