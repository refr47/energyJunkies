#pragma once
#include <Arduino.h>
#include <AsyncTCP.h>
#include "defines.h"

#include <ESPAsyncWebServer.h>
AsyncWebSocket *webSockets_init(CALLBACK_GET_DATA);
String getJsonObj();
void notifyClients(String jsonData);