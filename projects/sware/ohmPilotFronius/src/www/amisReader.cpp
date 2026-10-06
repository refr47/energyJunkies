#ifdef AMIS_READER_DEV

#include <ArduinoJson.h>
#include <JSONVar.h>
#include <JSON.h>
#include <ModbusIP_ESP8266.h>

#include "amisReader.h"
#include "debugConsole.h"
#include "utils.h"

#define PATH_NAME_AMIS "/rest"
#define AMISREADER_SMART_METER_PORT 502

KEY_VALUE_MAP_t amisKeyValueMap[AMIS_VALUE_COUNT] = {
    {"1.8.0", 0},
    {"2.8.0", 1},
    {"saldo", 2}

};

static IPAddress remote;
static ModbusIP mb;

static uint32_t read32BitRegister(uint32_t startReg);
static bool amiReader_isConnectedAndReconnect();
static bool amisReader_mb_init(Setup &setUpData);

/* MODBUS interface */

static uint32_t read32BitRegister(uint32_t startReg)
{
    uint16_t res[2];

    if (mb.readHreg(remote, startReg - 1, res, 2, nullptr, 1))
    {
        mb.task();
        vTaskDelay(pdMS_TO_TICKS(100));
        return (uint32_t)res[0] << 16 | res[1];
    }
    return 0;
}

static bool amiReader_isConnectedAndReconnect()
{
    bool success = true;
    if (!mb.isConnected(remote))
    {
        success = mb.connect(remote, AMISREADER_SMART_METER_PORT);
        vTaskDelay(pdMS_TO_TICKS(1000));
        mb.client();
        LOG_DEBUG(TAG_AMIS, "amisreader smartmeter do connect .... %x", success);

        if (!success)
        {
            LOG_ERROR(TAG_AMIS, "Error in connection to Inverter via Modbus: %s", strerror(errno));
        }
    }

    return success;
}

static bool amisReader_mb_init(Setup &setUpData)
{
    LOG_INFO(TAG_AMIS, "amisReader_mb_init");

    if (!remote.fromString(setUpData.amisReaderHost))
    {
        LOG_ERROR(TAG_AMIS, "mb_init:: - cannot convert IP-Adresse of Converter from string");
        return false;
    }

    return amiReader_isConnectedAndReconnect();
}

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
    LOG_INFO(TAG_AMIS, "amisReader_initRestTargets , HOST: %s", webSockData.setupData.amisReaderHost);
    sprintf(buf, "http://%s%s", webSockData.setupData.amisReaderHost, PATH_NAME_AMIS);
    uRL = buf;
    webSockData.states.amisReader = false;
    String json_array = util_GET_Request(uRL.c_str(), &httpResponseCode);
    if (httpResponseCode != 200)
    {
        LOG_ERROR(TAG_AMIS, "amisReader_initRestTargets:: AMIS Reader API nicht erreichbar - kein AMIS Reader?");
        return false;
    }
    bool success = amisReader_mb_init(webSockData.setupData);
    if (success)
    {
        webSockData.states.amisReader = true;
    }
    else
    {
        webSockData.states.amisReader = false;
    }

    return success;
    // return utils_sock_initRestTargets(setup, AMIS_READER_INDEX);
}

bool amisReader_readRestTarget(WEBSOCK_DATA &webSockData)
{

    int htppResponse = 0;
    String json_array = util_GET_Request(uRL.c_str(), &htppResponse);
    if (htppResponse != 200)
    {
        LOG_ERROR(TAG_AMIS, "amisReader_readRestTarget:: ResponsCode != 200");
        return false;
    }
    // DBGf("solar_get_powerFlow(): %s", json_array);
    JSONVar my_obj = JSON.parse(json_array);
    if (JSON.typeof(my_obj) == "undefined")
    {
        LOG_ERROR(TAG_AMIS, "Parsing input failed!");
        return false;
    }

    webSockData.amisReader.saldo = my_obj["saldo"];
    webSockData.amisReader.absolutExportInkWh = my_obj["2.8.0"]; // bissherige einspeisung insge
    webSockData.amisReader.absolutImportInkWh = my_obj["1.8.0"]; // gesamter Strombezug
    webSockData.amisReader.exportInWatt = my_obj["2.7.0"];       // wirkleistug P-, aktueller export
    webSockData.amisReader.consumptionInWatt = my_obj["1.7.0"];  // aktueller bezug

    uint16_t saldoWatt;
    uint32_t einspeisung, bezug;

    mb.readHreg(remote, 40097, &saldoWatt, 1, nullptr, 1);
    mb.task();
    vTaskDelay(pdMS_TO_TICKS(100));
    LOG_INFO(TAG_AMIS, "SaldoWatt: %d", saldoWatt);

    einspeisung = read32BitRegister(40130);
    bezug = read32BitRegister(40138);

    LOG_INFO(TAG_AMIS, "Einspeisung: %d, Bezug: %d", einspeisung, bezug);

    return true;

    /* return utils_sock_readRestTarget(webSockData, AMIS_READER_INDEX, mapJsonValues); */
}

#endif