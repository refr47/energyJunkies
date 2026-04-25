#include "webSockets.h"
#include "utils.h"
#include <Arduino_JSON.h>
// do not reodrder *.h-files !!
// project: https : // randomnerdtutorials.com/esp32-websocket-server-arduino/

/*
        *****************************************************
        DEFINES

*/
#define PRODUKTION "solEr"
#define EIGENVERBRAUCH "ev"
#define EINSPEISUNG "netzBezug"
#define TEMP_PUFFERSPEICHER "bTemp"

#define HEIZPATRONE_L3 "L3"
#define HEIZPATRONE_L2 "L2"
#define HEIZPATRONE_L1 "L1"
#define FEHLER "errors"
#define AAKU_AVAILABLE "aakHasBattery"
#define AKKU_CAPACITA "aakPower"
#define AKKU_ZUSTAND "aakStat"
#define AKKU_ENTLADEN "aakEntladen"

#define NETZ_EXPORT_INS "NEI"
#define NETZ_IMPORT_INS "NII"
#define FORCE_HEIZPATRONE "forceHeizung"
#define EPSILON_PIN_MANAGER "EpsilonPin"

#define STATE_CARDWRITE 0      // 1 << 0 = 1
#define STATE_FLASH 1          // 1 << 1 = 2
#define STATE_MODBUS 2         // 1 << 2 = 4
#define STATE_TEMPSENSOR 3     //      1 << 3 = 8
#define STATE_BOILER_HEATING 4 // 1 << 4 = 16
#define STATE_AMIS_READER 5    // 1 << 5 = 32
#define STATE_MQTT 6           // 1 << 6 = 64
#define STATE_INFLUX 7         // 1 << 7 = 128
#define STATE_WATT_BIAS 8      // 1 << 8 = 256

#define FORMAT_BUFFER_LEN 35
#define JSON_OBJECT_BUFFER_LEN 2048
//
//  using macro to convert float to string
#define STRING(Value) #Value
/*
        *****************************************************
        PROTOTYPES

*/
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

/*forceHeizung
        *****************************************************
        local variables

*/

static AsyncWebSocket ws("/ws");
// static JSONVar live; // Json Variable to Hold Sensor live
static CALLBACK_GET_DATA webSockData;
static char formatBuffer[FORMAT_BUFFER_LEN];
static char jsonObjBuffer[JSON_OBJECT_BUFFER_LEN];

// Create a WebSocket object static AsyncWebSocket ws("/ws");
static char *getJsonObj();

AsyncWebSocket *webSockets_init(CALLBACK_GET_DATA getData)
{
    ws.onEvent(onEvent);
    webSockData = getData;
    return &ws;
}

static double prevValueFromSmartMeter = 0.0;
static unsigned int bitMaster = 0;
static JsonDocument doc;

static char *getJsonObj()
{

    doc.clear();
    JsonObject live = doc.createNestedObject("live");
    if (live.isNull())
    {
        LOG_ERROR(TAG_WEB_SOCKETS, "Failed to create JSON object");
        return "{}";
    }

    WEBSOCK_DATA data = webSockData();

    if (data.states.froniusAPI)
    {
        live[PRODUKTION] = data.fronius_SOLAR_POWERFLOW.p_pv;
        live[EINSPEISUNG] = data.fronius_SOLAR_POWERFLOW.p_grid;
        live[EIGENVERBRAUCH] = data.fronius_SOLAR_POWERFLOW.p_load;
        // live[AKKU_AKKU] = (int)data.fronius_SOLAR_POWERFLOW.p_akku;
    }
    else if (data.states.modbusOK)
    {
        live[PRODUKTION] = data.mbContainer.inverterSumValues.data.acCurrentPower;
        // live[AKKU_AKKU] = 0;

        if (data.mbContainer.inverterSumValues.data.acCurrentPower + data.mbContainer.meterValues.data.acCurrentPower >= 0.0)
        {
            live[EINSPEISUNG] = data.mbContainer.meterValues.data.acCurrentPower;

            live[EIGENVERBRAUCH] = data.mbContainer.inverterSumValues.data.acCurrentPower + data.mbContainer.meterValues.data.acCurrentPower;
            prevValueFromSmartMeter = data.mbContainer.meterValues.data.acCurrentPower;
        }
        else
        {
            live[EINSPEISUNG] = prevValueFromSmartMeter;
            live[EIGENVERBRAUCH] = data.mbContainer.inverterSumValues.data.acCurrentPower + prevValueFromSmartMeter;
        }
    }
    else // amis reader
    {

        live[EINSPEISUNG] = data.amisReader.saldo;
        live[EIGENVERBRAUCH] = data.amisReader.consumptionInWatt;
        live[PRODUKTION] = data.amisReader.exportInWatt;
        // live[AKKU_AKKU] = 0;
    }
    int avgTemp = (data.temperature.sensor1 + data.temperature.sensor2) / 2;

    if (data.temperature.alarm)
    {

        if (data.temperature.sensor1 > 0 && data.temperature.sensor2 > 0)
        {
            snprintf(formatBuffer, FORMAT_BUFFER_LEN, "!! %d !! ", avgTemp);
        }
        else
        {
            snprintf(formatBuffer, FORMAT_BUFFER_LEN, "!! %d !! ", avgTemp);
        }
    }
    else
    {
        snprintf(formatBuffer, FORMAT_BUFFER_LEN, "%d", avgTemp);
    }

    live[TEMP_PUFFERSPEICHER] = formatBuffer;
    live[HEIZPATRONE_L1] = data.pidContainer.PID_PIN1;
    live[HEIZPATRONE_L2] = data.pidContainer.PID_PIN2;
    live[HEIZPATRONE_L3] = data.pidContainer.mAnalogOut;
    live[FORCE_HEIZPATRONE] = (int)data.setupData.forceHeating;

    live[AAKU_AVAILABLE] = data.setupData.akku;
    live[AKKU_CAPACITA] = data.mbContainer.akkuState.data.capacity;
    live[AKKU_ZUSTAND] = data.mbContainer.akkuStr.data.stateOfCharge;
    live[AKKU_ENTLADEN] = data.mbContainer.akkuStr.data.dischargeRate;

    bitMaster = 0;

    /*  if (!data.states.cardWriterOK)
         bitMaster |= (1 << STATE_CARDWRITE); */
    if (!data.states.flashOK)
        bitMaster |= (1 << STATE_FLASH);
#ifdef FRONIUS_IV
    if (!data.states.modbusOK)
        bitMaster |= (1 << STATE_MODBUS);
#endif

    if (!data.states.tempSensorOK)
        bitMaster |= (1 << STATE_TEMPSENSOR);
    if (!data.states.boilerHeating)
        bitMaster |= (1 << STATE_BOILER_HEATING);
#ifdef AMIS_READER_DEV
    if (!data.states.amisReader)
        bitMaster |= (1 << STATE_AMIS_READER);
#endif
#ifdef MQTT
    if (!data.states.mqtt)
        bitMaster |= (1 << STATE_MQTT);
#endif 
    if (data.states.wattBiasForTest)
        bitMaster |= (1 << STATE_WATT_BIAS);
        
    live[FEHLER] = bitMaster;

    /*  live[AKKU_LADEN] = data.mbContainer.akkuStr.data.chargeRate;
     live[AKKU_ENTLADEN] = data.mbContainer.akkuStr.data.dischargeRate; */

    // =========================
    // 🟡 LOG OBJECT
    // =========================
    /*  JsonObject logObj = doc.createNestedObject("log");
     JsonArray entries = logObj.createNestedArray("entries"); */
    RingBuffer &rb = data.logBuffer;
    LOG_DEBUG(TAG_WEB_SOCKETS, "Preparing JSON log entries, log buffer active: %d", rb.active);
    /*  if (!rb.active)
     {
         logObj["count"] = 0;
         size_t freeBytes = measureJson(doc);
         if (freeBytes >= JSON_OBJECT_BUFFER_LEN)
         {
             LOG_ERROR(TAG_WEB_SOCKETS, "Not enough memory to create JSON log entries");
             return "{}";
         }
         serializeJson(doc, jsonObjBuffer);
         jsonObjBuffer[freeBytes] = '\0'; // Null-terminator hinzufügen
             // DBGf("JSON-String: %s", jsonString.c_str());
             return jsonObjBuffer;
     } */

    LOG_DEBUG(TAG_WEB_SOCKETS, "Creating JSON log entries");
    int count = utils_logRead(rb, doc);
    // 🔑 count setzen
    // logObj["count"] = count;

    /* if (freeBytes >= JSON_OBJECT_BUFFER_LEN)
    {
        LOG_ERROR(TAG_WEB_SOCKETS, "Not enough memory to create JSON log entries");
        return "{}";
    } */
    serializeJson(doc, jsonObjBuffer);
    // jsonObjBuffer[freeBytes] = '\0';
    // LOG_DEBUG(TAG_WEB_SOCKETS, "JSON log entries created, free bytes: %s", doc.as<String>().c_str() );
    // LOG_DEBUG(TAG_WEB_SOCKETS, "Send stream with count: %d", count);
    return jsonObjBuffer;
}
void cleanupClients()
{
    ws.cleanupClients();
}

void notifyClients()
{
    if (ws.count() > 0)
    {
        ws.textAll(getJsonObj());
    }
    else
    {
        LOG_DEBUG(TAG_WEB_SOCKETS, "No clients connected, skipping notify");
    }
}
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        // data[len] = 0;
        // String message = (char*)data;
        //  Check if the message is "getlive"
        // if (strcmp((char*)data, "getlive") == 0) {
        // if it is, send current sensor live
        LOG_INFO(TAG_WEB_SOCKETS, "webSockets::handleWebSocketMessage for message: %s", (char *)data);
        /*  if (strcmp((char *)data, "getLifeData") == 0)
         { */

        notifyClients();
        //}
        //}
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}