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
#define AKKU "AKKU"
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
#define SIM_BIAS "SimBias"
#define SIM_LOAD "SimLoad"

#define HEIZPATRONE_L1 1
#define HEIZPATRONE_L2 2
#define AKKU_AVAILABLE 3
#define STATE_CARDWRITE 4
#define STATE_MODBUS 5
#define STATE_FLASH 6
#define STATE_TEMPSENSOR 7

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
static JSONVar readings; // Json Variable to Hold Sensor Readings
static CALLBACK_GET_DATA webSockData;
static char formatBuffer[35];
// Create a WebSocket object static AsyncWebSocket ws("/ws");

AsyncWebSocket *webSockets_init(CALLBACK_GET_DATA getData)
{
    ws.onEvent(onEvent);
    webSockData = getData;
    return &ws;
}

// Get Sensor Readings and return JSON object
/* String getSensorReadings()
{
    readings["temperature"] = String("1");
    readings["humidity"] = String("2");
    readings["pressure"] = String("3");
    String jsonString = JSON.stringify(readings);
    return jsonString;
} */

static double prevValueFromSmartMeter = 0.0;
static unsigned int bitMaster = 0;
String getJsonObj()
{
    WEBSOCK_DATA data = webSockData();
    DBGf("webSocks - getJsonOj:  %.2lf", data.temperature.sensor1);
#ifdef AMIS_READER_DEV
    readings[EINSPEISUNG] = data.amisReader.saldo;
#endif
#ifdef FRONIUS_API
    readings[PRODUKTION] = data.fronius_SOLAR_POWERFLOW.p_pv;
    readings[EINSPEISUNG] = data.fronius_SOLAR_POWERFLOW.p_grid;
    readings[EIGENVERBRAUCH] = data.fronius_SOLAR_POWERFLOW.p_load;
    readings[AKKU] = data.fronius_SOLAR_POWERFLOW.p_akku;

#else
    readings[PRODUKTION] = data.mbContainer.inverterSumValues.data.acCurrentPower;
    readings[AKKU] = 0;

    if (data.mbContainer.inverterSumValues.data.acCurrentPower + data.mbContainer.meterValues.data.acCurrentPower >= 0.0)
    {
        readings[EINSPEISUNG] = data.mbContainer.meterValues.data.acCurrentPower;

        readings[EIGENVERBRAUCH] = data.mbContainer.inverterSumValues.data.acCurrentPower + data.mbContainer.meterValues.data.acCurrentPower;
        prevValueFromSmartMeter = data.mbContainer.meterValues.data.acCurrentPower;
    }
    else
    {
        readings[EINSPEISUNG] = prevValueFromSmartMeter;
        readings[EIGENVERBRAUCH] = data.mbContainer.inverterSumValues.data.acCurrentPower + prevValueFromSmartMeter;
    }
#endif
    if (data.temperature.alarm)
    {
        if (data.temperature.sensor1 > 0.0 && data.temperature.sensor2 > 0.0)
        {
            sprintf(formatBuffer, "!!%.2f!!", (data.temperature.sensor1 + data.temperature.sensor2) / 2.0);
            // readings[TEMP_PUFFERSPEICHER] = (data.temperature.sensor1 + data.temperature.sensor2) / 2.0);
        }
        else
        {
            sprintf(formatBuffer, "!!%.2f %.2f", data.temperature.sensor1, data.temperature.sensor2);
        }
    }
    else
    {
        sprintf(formatBuffer, "%.2f", (data.temperature.sensor1 + data.temperature.sensor2) / 2.0);
    }

    readings[TEMP_PUFFERSPEICHER] = formatBuffer;
    bitMaster = 0;

    if (data.pidContainer.PID_PIN1 == 1)
        bitMaster |= (1 << HEIZPATRONE_L1);
    if (data.pidContainer.PID_PIN2 == 1)
        bitMaster |= (1 << HEIZPATRONE_L2);
    if (data.setup.externerSpeicher == true)
        bitMaster |= (1 << AKKU_AVAILABLE);
    if (!data.states.cardWriterOK)
        bitMaster |= (1 << STATE_CARDWRITE);
    if (!data.states.flashOK)
        bitMaster |= (1 << STATE_FLASH);
    if (!data.states.modbusOK)
        bitMaster |= (1 << STATE_MODBUS);
    if (!data.states.tempSensorOK)
        bitMaster |= (1 << STATE_TEMPSENSOR);
    readings[FEHLER] = bitMaster;

    readings[HEIZPATRONE_L3] = data.pidContainer.mAnalogOut;
    readings[MUSS_EINSPEISUNG] = data.pidContainer.powerNotUseable;
    readings[AKKU_CAPACITA] = data.mbContainer.akkuState.data.capacity;
    readings[AKKU_ZUSTAND] = data.mbContainer.akkuStr.data.stateOfCharge;
    readings[AKKU_LADEN] = data.mbContainer.akkuStr.data.chargeRate;
    readings[AKKU_ENTLADEN] = data.mbContainer.akkuStr.data.dischargeRate;
    readings[SIM_BIAS] = data.setup.exportWatt;
    readings[SIM_LOAD] = data.setup.additionalLoad;

    String jsonString = JSON.stringify(readings);
    // DBGf("JSON-String: %s", jsonString.c_str());
    return jsonString;
}
void notifyClients(String sensorReadings)
{
    ws.textAll(sensorReadings);
}
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        // data[len] = 0;
        // String message = (char*)data;
        //  Check if the message is "getReadings"
        // if (strcmp((char*)data, "getReadings") == 0) {
        // if it is, send current sensor readings
        DBGf("handleWebSocketMessage for message: %s", (char *)data);
        /*  if (strcmp((char *)data, "getLifeData") == 0)
         { */
        String sensorReadings = getJsonObj();
        Serial.print(sensorReadings);
        notifyClients(sensorReadings);
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