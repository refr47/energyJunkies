#ifdef AMIS_READER_DEV

#include <ArduinoJson.h>
#include <JSONVar.h>
#include <JSON.h>

#include "amisReader.h"
#include "debugConsole.h"
#include "utils.h"

#define PATH_NAME_AMIS "/rest"

KEY_VALUE_MAP_t amisKeyValueMap[AMIS_VALUE_COUNT] = {
    {"1.8.0", 0},
    {"2.8.0", 1},
    {"saldo", 2}

};

/* ****************** PROTOTYPES **********************************************/

// static bool readJsonResponse(HTTP_REST_TARGET_t *target, WEBSOCK_DATA &webSockData);
static String uRL = "";

static void mapJsonValues(HTTP_REST_TARGET_t *target, char jsonString[], WEBSOCK_DATA &webSockData);

// initialize the rest API targets
bool amisReader_initRestTargets(WEBSOCK_DATA &webSockData)
{
    char buf[70];
    memset(buf, 0, 70);
    int httpResponseCode = 0;
    DBGf("amisReader_initRestTargets , HOST: %s", webSockData.setupData.amisReaderHost);
    sprintf(buf, "http://%s%s", webSockData.setupData.amisReaderHost, PATH_NAME_AMIS);
    uRL = buf;
    webSockData.states.amisReader = false;
    String json_array = util_GET_Request(uRL.c_str(), &httpResponseCode);
    if (httpResponseCode != 200)
    {
        DBGf("solar_init:: AMIS Reader API nicht erreichbar - kein AMIS Reader?");
        return false;
    }
    webSockData.states.amisReader = true;
    return true;
    // return utils_sock_initRestTargets(setup, AMIS_READER_INDEX);
}

bool amisReader_readRestTarget(WEBSOCK_DATA &webSockData)
{

    int htppResponse = 0;
    String json_array = util_GET_Request(uRL.c_str(), &htppResponse);
    if (htppResponse != 200)
    {
        DBGf("amisReader_readRestTarget:: ResponsCode != 200");
        return false;
    }
    // DBGf("solar_get_powerFlow(): %s", json_array);
    JSONVar my_obj = JSON.parse(json_array);
    if (JSON.typeof(my_obj) == "undefined")
    {
        DBG("Parsing input failed!");
        return false;
    }

    webSockData.amisReader.saldo = my_obj["saldo"];
    webSockData.amisReader.absolutExportInkWh = my_obj["2.8.0"];
    webSockData.amisReader.absolutImportInkWh = my_obj["1.8.0"];
    return true;

    /* return utils_sock_readRestTarget(webSockData, AMIS_READER_INDEX, mapJsonValues); */
}

static void mapJsonValues(HTTP_REST_TARGET_t *target, char *jsonStart, WEBSOCK_DATA &webSockData)
{
    StaticJsonDocument<512> jsonBuffer;

    // DBGf("mapJsonValues   ENTER  %s", jsonStart);

    deserializeJson(jsonBuffer, jsonStart);
    // DBGf("mapJsonValues %s", jsonStart);

    for (int i = 0; i < target->valueCount; i++)
    {
#ifdef VERBOSE
        DBGf("map, key: %s", target->mapping[i].key);
        DBGf("map, jsonObj: %d", jsonBuffer[target->mapping[i].key]);
#endif
        switch (i)
        {
        case 0:
            webSockData.amisReader.absolutImportInkWh = jsonBuffer[target->mapping[i].key];
            break;
        case 1:
            webSockData.amisReader.absolutExportInkWh = jsonBuffer[target->mapping[i].key];
            break;
        case 2:
            webSockData.amisReader.saldo = jsonBuffer[target->mapping[i].key];
            break;
        default:
            DBGf("amisReader::mapJsonValues() no mapping found for index: %d", i);
        }
    }

    DBGf("mapJsonValues: Wirkleistung P+ %.3f", webSockData.amisReader.absolutImportInkWh);
}

#endif