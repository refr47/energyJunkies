#pragma once

/* Reihenfolge ist wichtig */
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncJson.h>

#include "debugConsole.h"
#include "defines.h"



void ajaxCalls_init(CALLBACK_GET_DATA getData, CALLBACK_SET_SETUP_CHANGED setupCh);

void ajaxCalls_handleGetSetup(AsyncWebServerRequest *request);
void ajaxCalls_handleGetOverview(AsyncWebServerRequest *request);
void ajaxCalls_handleStoreSetup(JsonDocument &json, AsyncWebServerRequest *request, bool isAPModus);
void ajaxCalls_handleGetFullSetup(AsyncWebServerRequest *request);

/* Shelly:
 * 1) ajaxCalls_triggerShellyScan() startet RTOS-Task
 * 2) ajaxCalls_handleBuildAndGetShelly() liefert letzten Snapshot zurück
 */
bool ajaxCalls_triggerShellyScan(void);
void ajaxCalls_handleBuildAndGetShelly(AsyncWebServerRequest *request);