#define __ACCESS_POINT_CPP

#include <WiFi.h>
#include <LittleFS.h>
#include <mdns.h>
#include <Update.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "tft.h"
#include "eprom.h"
#include "utils.h"
#include "ajaxCalls.h"
#include "www.h"
#include "webSockets.h"
#include "logReader.h"
#include "loxoneProvider.h"

// --- Web-Login-Credentials (in Produktivbetrieb aendern!) ---
#ifndef HTTP_WEB_USER
#define HTTP_WEB_USER "admin"
#endif
#ifndef HTTP_WEB_PASS
#define HTTP_WEB_PASS "password"
#endif

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me−no−dev/arduino−esp32fs−plugin */

/*
 ***********************  DEFINES
 */

#define FORMAT_SPIFFS_IF_FAILED true

#define SSID_FOR_ACCESS_POINT "EnergieJunkies"
/*
 ***********************  prototypes


/*
 ***********************  local variables
 */

static AsyncWebServer server(80);

// Flag to check if the user is authenticated
static bool isAuthenticated = false;
static bool isAPModus = false; // only in APModus a reboot is required

static void handleLogin(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{

    LOG_INFO(TAG_WEB, "handleLogin::begin");
    JsonDocument doc;
    deserializeJson(doc, data, len);

    // Check if the request method is POST

    const char *user = doc["user"] | "";
    const char *pass = doc["password"] | "";
    // LOG_INFO(TAG_WEB, "handleLogin::user: %s, pass: %s", user, pass);
    //  Check if the POST data matches the expected username and password
    if (strcmp(user, HTTP_WEB_USER) == 0 && strcmp(pass, HTTP_WEB_PASS) == 0)
    {
        isAuthenticated = true;
        request->send(200, "application/json", "{\"authenticated\":true}");
        LOG_INFO(TAG_WEB, "handleLogin::authenticated");
    }
    else
    {
        request->send(401, "application/json", "{\"error\":\"Benutzername oder Passwort falsch\"}");
        LOG_INFO(TAG_WEB, "handleLogin::authentication failed");
    }
}

static void handleRoot(AsyncWebServerRequest *request)
{
    // Check if the user is authenticated

    if (isAuthenticated)
        request->send(LittleFS, "/index.html", String(), false);
    else
        request->send(LittleFS, "/login.html", "text/html", false);
}

static void handleSetup(AsyncWebServerRequest *request)
{
    // Check if the user is authenticated
    if (isAuthenticated)
        request->send(LittleFS, "/setupData.html", "text/html", false);
    else
        request->send(LittleFS, "/login.html", "text/html", false);
}

static inline bool helperForWWW()
{
    if (!LittleFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        LOG_ERROR(TAG_WEB, "www_init::LITTLEFS Mount Failed");
        return false;
    }
    // In deiner helperForWWW Funktion oder dort, wo du iterierst:
    /*   File root = LittleFS.open("/");
      File entry = root.openNextFile();

      while (entry)
      {
          if (entry.isDirectory())
          {
              // Überspringe Ordner, da sie keine "Dateien" im herkömmlichen Sinne sind
              entry = root.openNextFile();
              continue;
          }

          // Nur wenn es KEIN Directory ist, darfst du auf Name/Size zugreifen
          Serial.printf("Datei: %s, Größe: %u\n", entry.name(), entry.size());

          entry = root.openNextFile();
      }
      root.close(); */
    return true;
}

static void inline doCORS()
{
#ifdef CORS_DEBUG
    // CORS Header global aktivieren
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
#endif
}

bool www_init(Setup &setupData, char *ipAddr, char *wlanAsClientSSID, CALLBACK_GET_DATA webSockData, CALLBACK_SET_SETUP_CHANGED setupChanged)
{
    // 1. LITTLEFS Initialisierung
    LOG_INFO(TAG_WEB, "www_init::begin");
    if (!helperForWWW())
    {
        return false;
    }

    // 2. Netzwerk-Modus (AP oder Client)
    if (ipAddr == NULL)
    {
        isAPModus = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(SSID_FOR_ACCESS_POINT, DEFAULT_IP_ACCESS_POINT);
        ipAddr = (char *)DEFAULT_IP_ACCESS_POINT;
        strcpy(setupData.currentIP, ipAddr);
    }
    else
    { 
        isAPModus = false;
    }
    doCORS();
    /// AJAX & API Endpoints
    server.on("/login", HTTP_OPTIONS, [](AsyncWebServerRequest *request)
              { request->send(200); });
    server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleLogin);
    server.on("/getSetup", HTTP_GET, ajaxCalls_handleGetSetup);
    server.on("/storeSetup", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                  // Dieser Teil bleibt leer, da wir den Body-Handler nutzen
              },
              NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
                // 1. Alle Chunks sammeln (_tempObject = Body-Puffer)
                if (index == 0) {
                    request->_tempObject = nullptr;
                }
                char *buf = (char *)request->_tempObject;
                if (!buf) {
                    buf = new (std::nothrow) char[total + 1];
                    if (!buf) {
                        request->send(500, "application/json", "{\"done\":false, \"msg\":\"ENOMEM\"}");
                        request->_tempObject = nullptr;
                        return;
                    }
                    request->_tempObject = buf;
                }
                if (buf && index + len <= (size_t)total) {
                    memcpy(buf + index, data, len);
                    buf[index + len] = '\0';
                }
                if (index + len == total) {
                    // BUGFIX: JSON aus GESAMTLICHEN Body, nicht aus letztem Chunk
                    JsonDocument doc;
                    DeserializationError error = (buf) ? deserializeJson(doc, buf) : deserializeJson(doc, data, len);
                    LOG_INFO(TAG_WEB, "/storeSetup: len: %u , index: %u, total: %u", len, index, total);
                    if (!error)
                    {
                        LOG_INFO(TAG_WEB, "/storeSetup: JSON OK");
                        ajaxCalls_handleStoreSetup(doc, request, isAPModus);
                    }
                    else
                    { 
                        LOG_INFO(TAG_WEB, "/storeSetup: JSON Error");
                        request->send(400, "application/json", "{\"done\":false, \"msg\":\"JSON Error\"}");
                    }
                    request->_tempObject = nullptr;
                    if (buf) delete[] buf;
                } });

    server.serveStatic("/assets/", LittleFS, "/assets/")
        .setCacheControl("max-age=600");

    // Falls du Bilder direkt im /img/ Ordner hast:
    server.serveStatic("/img/", LittleFS, "/img/")
        .setCacheControl("max-age=3600");

    // WICHTIG: Die Root-Datei (index.html)
    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html");

    // --- FEHLER-HANDLING & AUTHENTIFIZIERUNG ---
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
    if (request->method() == HTTP_GET) {
        request->send(LittleFS, "/index.html", "text/html");
    } else {
        request->send(404);
    } });

    // Initialisierung abschließen
    ajaxCalls_init(webSockData, setupChanged);
    server.addHandler(webSockets_init(webSockData));
    loxoneProvider_register(server);
    server.begin();

    LOG_INFO(TAG_WEB, "WebServer started. Free Heap: %u", ESP.getFreeHeap());
    return true;
}

void www_run()
{
}


