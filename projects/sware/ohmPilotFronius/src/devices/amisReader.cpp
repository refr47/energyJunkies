#ifdef AMIS_READER_DEV

#include <ArduinoJson.h>
#include "amisReader.h"
#include "debugConsole.h"
#include "utils.h"

KEY_VALUE_MAP_t amisKeyValueMap[AMIS_VALUE_COUNT] = {
    {"1.8.0", 0},
    {"2.8.0", 1},
    {"saldo", 2}

};

/* ****************** PROTOTYPES **********************************************/

// static bool readJsonResponse(HTTP_REST_TARGET_t *target, WEBSOCK_DATA &webSockData);
static void mapJsonValues(HTTP_REST_TARGET_t *target, char jsonString[], WEBSOCK_DATA &webSockData);

// initialize the rest API targets
bool amisReader_initRestTargets(Setup &setup)
{
    DBGf("amisReader_initRestTargets , HOST: %s", setup.amisReaderHost);
    return utils_sock_initRestTargets(setup.amisReaderHost, AMIS_READER_INDEX);
}

bool amisReader_readRestTarget(WEBSOCK_DATA &webSockData)
{

    return utils_sock_readRestTarget(webSockData, AMIS_READER_INDEX, mapJsonValues);
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