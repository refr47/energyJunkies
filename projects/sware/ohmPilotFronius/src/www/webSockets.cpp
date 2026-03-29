#include "webSockets.h"
#include "utils.h"
#include <Arduino_JSON.h>
// do not reodrder *.h-files !!
// project: https : // randomnerdtutorials.com/esp32-websocket-server-arduino/

/*
        *****************************************************
        DEFINES

*/
#define PRODUKTION "PR"
#define EIGENVERBRAUCH "EV"
#define EINSPEISUNG "EINS"
#define AKKU_AKKU "AKKU"
#define TEMP_PUFFERSPEICHER "TPS"

#define HEIZPATRONE_L3 "HL3"
#define MUSS_EINSPEISUNG "ME"
#define FEHLER "FE"

#define AKKU_CAPACITA "AkCap"
#define AKKU_ZUSTAND "AkStat"
#define AKKU_LADEN "AkLoad"
#define AKKU_ENTLADEN "AkDis"
#define NETZ_EXPORT_INS "NEI"
#define NETZ_IMPORT_INS "NII"
#define FORCE_HEIZPATRONE "Force Heizpatrone"
#define EPSILON_PIN_MANAGER "EpsilonPin"

#define HEIZPATRONE_L1 1
#define HEIZPATRONE_L2 2
#define AKKU_AVAILABLE 3
#define STATE_CARDWRITE 4
#define STATE_MODBUS 5
#define STATE_FLASH 6
#define STATE_TEMPSENSOR 7
#define STATE_BOILER_HEATING 8

//
//  using macro to convert float to string
#define STRING(Value) #Value
/*
        *****************************************************
        PROTOTYPES

*/
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

/*
        *****************************************************
        local variables

*/
static AsyncWebSocket ws("/ws");
// static JSONVar live; // Json Variable to Hold Sensor live
static CALLBACK_GET_DATA webSockData;
static char formatBuffer[35];

// Create a WebSocket object static AsyncWebSocket ws("/ws");

AsyncWebSocket *webSockets_init(CALLBACK_GET_DATA getData)
{
    ws.onEvent(onEvent);
    webSockData = getData;
    return &ws;
}

static double prevValueFromSmartMeter = 0.0;
static unsigned int bitMaster = 0;
static DynamicJsonDocument doc(2048);

String getJsonObj()
{
  
    doc.clear();
    JsonObject live = doc.createNestedObject("live");
    if (live.isNull())
    {
        LOG_ERROR("Failed to create JSON object");
        return "{}";
    }

    WEBSOCK_DATA data = webSockData();

    if (data.states.froniusAPI)
    {
        live[PRODUKTION] = data.fronius_SOLAR_POWERFLOW.p_pv;
        live[EINSPEISUNG] = data.fronius_SOLAR_POWERFLOW.p_grid;
        live[EIGENVERBRAUCH] = data.fronius_SOLAR_POWERFLOW.p_load;
        live[AKKU_AKKU] = (int)data.fronius_SOLAR_POWERFLOW.p_akku;
    }
    else if (data.states.modbusOK)
    {
        live[PRODUKTION] = data.mbContainer.inverterSumValues.data.acCurrentPower;
        live[AKKU_AKKU] = 0;

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
        live[AKKU_AKKU] = 0;
    }
    
    if (data.temperature.alarm)
    {
        if (data.temperature.sensor1 > 0.0 && data.temperature.sensor2 > 0.0)
        {
            sprintf(formatBuffer, "!! %.2f !! ", (data.temperature.sensor1 + data.temperature.sensor2) / 2.0);
        }
        else
        {
            sprintf(formatBuffer, "!! %.2f !! ", (data.temperature.sensor1 + data.temperature.sensor2) / 2.0);
        }
    }
    else
    {
        sprintf(formatBuffer, "%.2f", (data.temperature.sensor1 + data.temperature.sensor2) / 2.0);
    }
 
    live[TEMP_PUFFERSPEICHER] = formatBuffer;
    bitMaster = 0;

    LOG_INFO("PID_PIN1: %d, PID_PIN2: %d, externerSpeicher: %d, cardWriterOK: %d, flashOK: %d, modbusOK: %d, tempSensorOK: %d, boilerHeating: %d",
             data.pidContainer.PID_PIN1,
             data.pidContainer.PID_PIN2,
             data.setupData.externerSpeicher,
             data.states.cardWriterOK,
             data.states.flashOK,
             data.states.modbusOK,
             data.states.tempSensorOK,
             data.states.boilerHeating);

             
    if (data.pidContainer.PID_PIN1 == 1)
        bitMaster |= (1 << HEIZPATRONE_L1);
    if (data.pidContainer.PID_PIN2 == 1)
        bitMaster |= (1 << HEIZPATRONE_L2);
    if (data.setupData.externerSpeicher == true)
        bitMaster |= (1 << AKKU_AVAILABLE);
    if (!data.states.cardWriterOK)
        bitMaster |= (1 << STATE_CARDWRITE);
    if (!data.states.flashOK)
        bitMaster |= (1 << STATE_FLASH);
    if (!data.states.modbusOK)
        bitMaster |= (1 << STATE_MODBUS);
    if (!data.states.tempSensorOK)
        bitMaster |= (1 << STATE_TEMPSENSOR);
    if (!data.states.boilerHeating)
        bitMaster |= (1 << STATE_BOILER_HEATING);

    live[FEHLER] = bitMaster;

    live[HEIZPATRONE_L3] = data.pidContainer.mAnalogOut;
    live[MUSS_EINSPEISUNG] = data.pidContainer.powerNotUseable;
    live[AKKU_CAPACITA] = data.mbContainer.akkuState.data.capacity;
    live[AKKU_ZUSTAND] = data.mbContainer.akkuStr.data.stateOfCharge;
    /*  live[AKKU_LADEN] = data.mbContainer.akkuStr.data.chargeRate;
     live[AKKU_ENTLADEN] = data.mbContainer.akkuStr.data.dischargeRate; */
    live[FORCE_HEIZPATRONE] = (int)data.setupData.forceHeating;
    live[EPSILON_PIN_MANAGER] = data.setupData.epsilonML_PinManager;

    // =========================
    // 🟡 LOG OBJECT
    // =========================
    JsonObject logObj = doc.createNestedObject("log");
    JsonArray entries = logObj.createNestedArray("entries");
    RingBuffer &rb = data.logBuffer;
    if (!rb.active)
    {
        logObj["count"] = 0;
        String jsonString;
        serializeJson(doc, jsonString);
        // DBGf("JSON-String: %s", jsonString.c_str());
        return jsonString;
    }
    uint16_t localRead = rb.readIndex;
    uint16_t localWrite = rb.writeIndex;

    uint16_t count = 0;

    while ((localRead != localWrite) && count < 30) // safety check to prevent infinite loop
    {
        LogEntry &e = rb.buffer[localRead];

        JsonObject obj = entries.createNestedObject();

        obj["ts"] = e.ts;
        obj["l1"] = e.state & 1;
        obj["l2"] = (e.state >> 1) & 1;
        obj["pwm"] = e.pwm;
        obj["temp"] = e.temp;

        localRead = (localRead + 1) % LOG_BUFFER_SIZE;
        count++;
    }

    // 🔑 count setzen
    logObj["count"] = count;
    String jsonString;
    serializeJson(doc, jsonString);
    // DBGf("JSON-String: %s", jsonString.c_str());
    return jsonString;
}


void notifyClients(String sensorlive)
{
    ws.textAll(sensorlive);
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
        LOG_INFO("webSockets::handleWebSocketMessage for message: %s", (char *)data);
        /*  if (strcmp((char *)data, "getLifeData") == 0)
         { */
        String sensorlive = getJsonObj();
        Serial.print(sensorlive);
        notifyClients(sensorlive);
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