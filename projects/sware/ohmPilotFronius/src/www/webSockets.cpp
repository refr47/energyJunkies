
#include "webSockets.h"
#include <Arduino_JSON.h>

using namespace std;
// https://randomnerdtutorials.com/esp32-websocket-server-arduino/
//  using macro to convert float to string
#define STRING(Value) #Value

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

// Create a WebSocket object
static AsyncWebSocket ws("/ws");
static JSONVar readings; // Json Variable to Hold Sensor Readings
static CALLBACK_GET_DATA webSockData;

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

String getJsonObj()
{
    WEBSOCK_DATA data = webSockData();
    DBGf("webSocks - getJsonOj:  %.2lf", data.temperature.sensor1);
    readings["TEMPERATUR"] = std::to_string(data.temperature.sensor1).c_str();
    DBGf("TEM in getJson: %s", std::to_string(data.temperature.sensor1).c_str());
    String jsonString = JSON.stringify(readings);
    DBGf("JSON-String: %s", jsonString.c_str());
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