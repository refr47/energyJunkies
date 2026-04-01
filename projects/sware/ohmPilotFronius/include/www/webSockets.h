#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "defines.h"

AsyncWebSocket *webSockets_init(CALLBACK_GET_DATA);

void notifyClients();