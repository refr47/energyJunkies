#include "froniusSolarAPI.h"

#include "utils.h"
#include <Arduino_JSON.h>
#include "defines.h"
#include "debugConsole.h"

#define PATH_NAME_FORECAST "/solar_api/v1/GetPowerFlowRealtimeData.fcgi"

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

    char buf[70];
    memset(buf, 0, 70);
    int httpResponseCode = 0;
    LOG_INFO(TAG_SOLAR ,"solar_init BEGIN ");
    // p_solarValid = &webSockData.states.froniusAPI;

    sprintf(buf, "http://%s%s", webSockData.setupData.inverter, PATH_NAME_FORECAST);
    uRL = buf;
    LOG_DEBUG(TAG_SOLAR,"solar_init() for %s", buf);
    webSockData.setupData.akku = false;
    *akku = false;
    webSockData.states.device = false;

    String json_array = util_GET_Request(uRL.c_str(), &httpResponseCode);
    if (httpResponseCode != 200)
    {
        LOG_ERROR(TAG_SOLAR,"solar_init:: Fronius API nicht erreichbar - kein Fronius Inverter?");
        return false;
    }

    else
    {
        LOG_DEBUG(TAG_SOLAR,"solar_init:: Got json: %s", json_array.c_str());
    }

    JSONVar my_obj = JSON.parse(json_array);
    if (JSON.typeof(my_obj) == "undefined")
    {
         LOG_ERROR(TAG_SOLAR,"Parsing input failed!");
        return false;
    }
    webSockData.states.device = true;
    if (JSON.typeof(my_obj["Body"]["Data"]["Site"]["P_Akku"]) == "undefined")
    {
        LOG_DEBUG(TAG_SOLAR,"solar_init Akku is not available!!");
    }
    else
    {
        webSockData.setupData.akku = 1;
        *akku = true;
        LOG_DEBUG(TAG_SOLAR,"solar_init - akku  vorhanden!");
    }
    return true;

}

bool solar_get_powerflow(WEBSOCK_DATA &webSockData)
{

#ifdef VERBOSE
    LOG_DEBUG("solar_get_powerflow() - uRL: %s", uRL.c_str());
#endif
    int htppResponse = 0;
    String json_array = util_GET_Request(uRL.c_str(), &htppResponse);
    if (htppResponse != 200)
    {
        LOG_ERROR(TAG_SOLAR,"solar_get_powerflow:: ResponsCode != 200");
        return false;
    }
    // DBGf("solar_get_powerFlow(): %s", json_array);
    JSONVar my_obj = JSON.parse(json_array);
    if (JSON.typeof(my_obj) == "undefined")
    {
         LOG_ERROR(TAG_SOLAR,"Parsing input failed!");
        return false;
    }

    JSONVar siteObj = my_obj["Body"]["Data"]["Site"];
    webSockData.fronius_SOLAR_POWERFLOW.p_akku = siteObj["P_Akku"]; // > 0: Bezug vom akku,  <0: akku laden
    webSockData.fronius_SOLAR_POWERFLOW.p_grid = siteObj["P_Grid"]; // öffentliches netz: <0 = einspeisung, >0 bezug
    webSockData.fronius_SOLAR_POWERFLOW.p_load = siteObj["P_Load"];  // haus verbrauch < 0: verbrauch > 0 einspeisung
    webSockData.fronius_SOLAR_POWERFLOW.p_pv   = siteObj["P_PV"]; // PV-Leistung (W)
    webSockData.fronius_SOLAR_POWERFLOW.rel_Autonomy         = siteObj["rel_Autonomy"];
    webSockData.fronius_SOLAR_POWERFLOW.rel_SelfConsumption  = siteObj["rel_SelfConsumption"];

    // ── Inverter-Daten (Batterie Ladezustand + Ladeleistung) ──
    JSONVar invObj = my_obj["Body"]["Data"]["Inverters"]["1"];
    if (JSON.typeof(invObj) == "object") {
        JSONVar invP      = invObj["P"];       // Ladeleistung Batterie (W)
        JSONVar invSOC    = invObj["SOC"];     // Ladezustand Batterie (%)
        JSONVar invEDay   = invObj["E_Day"];   // Tagesertrag (Wh) – kann null sein
        JSONVar invETotal = invObj["E_Total"]; // Gesamtertrag (Wh)

      
        webSockData.fronius_SOLAR_POWERFLOW.soc     = invSOC;  // ladezustand batterie in % (0-100)

        // E_Day kann null sein (nachts / kein PV-Betrieb)
        if (JSON.typeof(invEDay) != "null" && JSON.typeof(invEDay) != "undefined")
            webSockData.fronius_SOLAR_POWERFLOW.e_day   = invEDay;
        else
            webSockData.fronius_SOLAR_POWERFLOW.e_day   = 0.0;

        webSockData.fronius_SOLAR_POWERFLOW.e_total = invETotal;
    }

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
#ifdef CCC
    return utils_sock_readRestTarget(webSockData, AMIS_READER_INDEX, mapJsonValuesFronius);
#endif
}

static void mapJsonValuesFronius(HTTP_REST_TARGET_t *target, char *jsonStart, WEBSOCK_DATA &webSockData)
{
    DynamicJsonDocument jsonBuffer(512);
    LOG_ERROR(TAG_SOLAR,"froniusSolarAPI::mapJsonValuesFronius ENTER");

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
            webSockData.fronius_SOLAR_POWERFLOW.p_akku = jsonBuffer["Body"]["Data"]["Site"]["P_Akku"];
            break;
        case 1:
            webSockData.fronius_SOLAR_POWERFLOW.p_grid = jsonBuffer["Body"]["Data"]["Site"]["P_Grid"];
            break;
        case 2:
            webSockData.fronius_SOLAR_POWERFLOW.p_load = jsonBuffer["Body"]["Data"]["Site"]["P_Load"];
            break;
        case 3:
            webSockData.fronius_SOLAR_POWERFLOW.p_pv = jsonBuffer["Body"]["Data"]["Site"]["P_PV"];
            break;
        case 4:
            webSockData.fronius_SOLAR_POWERFLOW.rel_Autonomy = jsonBuffer["Body"]["Data"]["Site"]["rel_Autonomy"];
            break;
        case 5:
            webSockData.fronius_SOLAR_POWERFLOW.rel_SelfConsumption = jsonBuffer["Body"]["Data"]["Site"]["rel_SelfConsumption"];
            break;
        default:
            LOG_ERROR(TAG_SOLAR,"solar_get_powerflow::mapJsonValues() no mapping found for index: %d", i);
        }
        LOG_ERROR(TAG_SOLAR,"froniusSolarAPI::mapJsonValuesFronius ExIT");
    }
}

