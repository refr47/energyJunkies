#define __ACCESS_POINT_CPP

#include <WiFi.h>
#include <SPIFFS.h>
#include <mdns.h>
#include <Update.h>

#include "tft.h"
#include "eprom.h"
#include "utils.h"
#include "ajaxCalls.h"
#include "www.h"
#include "webSockets.h"
#include "logReader.h"

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

static void sendHTML(String &path, AsyncWebServerRequest *request);
static void listDir(char *dir);

/*
 ***********************  local variables
 */

static AsyncWebServer server(80);

// Flag to check if the user is authenticated
static bool isAuthenticated = false;
static bool isAPModus = false; // only in APModus a reboot is required

static void listDir(char *dir)
{
    LOG_INFO("www::listdir");
    File root = SPIFFS.open(dir);

    if (!root)
    {
        LOG_ERROR("www::- failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        LOG_ERROR("www:: - not a directory");
        return;
    }

    LOG_INFO("www::%s", root);
    File file = root.openNextFile();

    while (file)
    {

        LOG_INFO("www::FILE: %s", file.name());

        file = root.openNextFile();
    }
}

static void sendHTML(String &path, AsyncWebServerRequest *request)
{
    if (SPIFFS.exists(path))
    {
        AsyncWebServerResponse *response = request->beginResponse(SPIFFS, path, String(), true);
        response->addHeader("Cache-Control", "max-age=600");
        request->send(response);
    }
    else
    {
        request->send(404, "text/plain", "File not found");
    }
}

static void handleLogin(AsyncWebServerRequest *request)
{

    bool standingData = false;
    // Check if the request method is POST
    if (request->method() == HTTP_POST)
    {
        // Check if the POST data matches the expected username and password
        if (
            request->hasParam("username", true) &&
            request->hasParam("password", true) &&
            // request->hasParam("standingData", true) &&
            request->getParam("username", true)->value() == "admin" &&
            request->getParam("password", true)->value() == "password")
        {
            isAuthenticated = true;
            standingData = request->arg("standingData") == "true";

            if (standingData)
                request->send(SPIFFS, "/setupData.html", String(), false);
            else
                request->send(SPIFFS, "/index.html", String(), false);
        }
        else
        {
            request->send(401, "text/plain", "Login failed");
        }
    }
    else
    {
        request->send(405, "text/plain", "Method Not Allowed");
    }
}

static void handleRoot(AsyncWebServerRequest *request)
{
    // Check if the user is authenticated

    if (isAuthenticated)
        request->send(SPIFFS, "/index.html", String(), false);
    else
        request->send(SPIFFS, "/login.html", "text/html", false);
}

static void handleSetup(AsyncWebServerRequest *request)
{
    // Check if the user is authenticated
    if (isAuthenticated)
        request->send(SPIFFS, "/setupData.html", "text/html", false);
    else
        request->send(SPIFFS, "/login.html", "text/html", false);
}


bool www_init(Setup &setupData, char *ipAddr, char *wlanAsClientSSID, CALLBACK_GET_DATA webSockData, CALLBACK_SET_SETUP_CHANGED setupChanged)
{
    // 1. SPIFFS Initialisierung
    LOG_INFO("www_init::begin");
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        LOG_ERROR("www_init::SPIFFS Mount Failed");
        return false;
    }
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        LOG_INFO("SPIFFS File: %s, Size: %u\n", file.name(), file.size());
        file = root.openNextFile();
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

    // --- STATISCHE DATEIEN (Ordner-basiert) ---
    // Das ersetzt ca. 20 einzelne server.on() Aufrufe!
    server.serveStatic("/css/", SPIFFS, "/css/")
        .setCacheControl("max-age=600");
    server.serveStatic("/js/", SPIFFS, "/js/").setCacheControl("max-age=600");
    server.serveStatic("/img/", SPIFFS, "/img/").setCacheControl("max-age=3600");

    // --- HTML ROUTEN ---
    server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", "Pong"); });
    server.on("/", HTTP_GET, handleRoot);
    server.on("/setup", HTTP_GET, handleSetup);
    server.on("/login", HTTP_POST, handleLogin);

    // Kurze Aliase für wichtige HTML-Seiten
    server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/about.html", "text/html"); });
    server.on("/shelly", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/shelly.html", "text/html"); });
    server.on("/out", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/output.html", "text/html"); });
    // Route für die Update-Seite (HTML)
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/update.html", "text/html"); });
    // --- LOGGING & DIAGNOSE ---
    server.on("/serial", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", logReader_getBufferAsString()); });
    server.on("/logOn", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
        logReader_enDisableRedirect(true);
        request->send(200, "text/plain", "Enabled"); });

    // --- AJAX & API Schnittstellen ---
    server.on("/getSetup", HTTP_GET, ajaxCalls_handleGetSetup);
    server.on("/getOverview", HTTP_GET, ajaxCalls_handleGetOverview);
    server.on("/buildAndGetShellyDevicesTree", HTTP_GET, ajaxCalls_handleBuildAndGetShelly);

    // Der eigentliche Upload-Handler
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    // Wird nach dem Upload aufgerufen
    bool updateFailed = Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (updateFailed) ? "Update Failed" : "Update Success! Rebooting...");
    response->addHeader("Connection", "close");
    request->send(response);
    
    if (!updateFailed) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
              {
    // Dieser Teil verarbeitet die Datenpakete (Chunks)
    if (!index) {
        LOG_INFO("Update Start: %s", filename.c_str());
        // Prüfen, ob es ein Filesystem oder Firmware Update ist
        int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
            Update.printError(Serial);
        }
    }
    
    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    
    if (final) {
        if (Update.end(true)) {
            LOG_INFO("Update Success: %u bytes", index + len);
        } else {
            Update.printError(Serial);
        }
    } });
    // JSON Handler für StoreSetup (Wichtig: Heap-Dokument nutzen!)
    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/storeSetup",
                                                                           [](AsyncWebServerRequest *request, JsonVariant &json)
                                                                           {
                                                                               ajaxCalls_handleStoreSetup(request, json, isAPModus);
                                                                           });
    server.addHandler(handler);

    // --- FEHLER-HANDLING & AUTHENTIFIZIERUNG ---
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
                        String path = request->url();
    
    // 1. Standard-Index fix
    if (path == "/") path = "/index.html";

    // 2. Debug-Ausgabe: Was wird gesucht?
    Serial.printf("Suche Datei: %s\n", path.c_str());

    if (SPIFFS.exists(path)) {
        request->send(SPIFFS, path);
    } else {
        Serial.printf("DATEI NICHT GEFUNDEN: %s\n", path.c_str());
        request->send(404, "text/plain", "404: File Not Found on SPIFFS");
    } });

    // Initialisierung abschließen
    ajaxCalls_init(webSockData, setupChanged);
    server.addHandler(webSockets_init(webSockData));
    server.begin();

    LOG_INFO("WebServer started. Free Heap: %u", ESP.getFreeHeap());
    return true;
}

void www_run()
{
}

#ifdef III
void www_run()
{
    WiFiClient client = server.available(); // Listen for incoming clients

    if (client)
    {                            // If a new client connects,
        DBGln("New Client.");    // print a message out in the serial port
        String currentLine = ""; // make a String to hold incoming data from the client
        while (client.connected())
        { // loop while the client's connected
            if (client.available())
            {                           // if there's bytes to read from the client,
                char c = client.read(); // read a byte, then
                Serial.write(c);        // print it out the serial monitor
                header += c;
                if (c == '\n')
                { // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0)
                    {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println("Connection: close");
                        client.println();

                        // turns the GPIOs on and off
                        if (header.indexOf("GET /26/on") >= 0)
                        {
                            output26State = "on";
                            DBGln("GPIO 26 on");
                        }
                        else if (header.indexOf("GET /26/off") >= 0)
                        {
                            DBGln("GPIO 26 off");
                            output26State = "off";
                        }
                        else if (header.indexOf("GET /27/on") >= 0)
                        {
                            DBGln("GPIO 27 on");
                        }
                        else if (header.indexOf("GET /27/off") >= 0)
                        {
                            DBGln("GPIO 27 off");
                        }

                        // Display the HTML web page
                        client.println("<!DOCTYPE html><html>");
                        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        client.println("<link rel=\"icon\" href=\"data:,\">");
                        // CSS to style the on/off buttons
                        // Feel free to change the background-color and font-size attributes to fit your preferences
                        client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
                        client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
                        client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
                        client.println(".button2 {background-color: #555555;}</style></head>");

                        // Web Page Heading
                        client.println("<body><h1>ESP32 Web Server</h1>");

                        // Display current state, and ON/OFF buttons for GPIO 26
                        client.println("<p>GPIO 26 - State " + output26State + "</p>");
                        // If the output26State is off, it displays the ON button
                        if (output26State == "off")
                        {
                            client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
                        }
                        else
                        {
                            client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
                        }

                        // Display current state, and ON/OFF buttons for GPIO 27
                        client.println("<p>GPIO 27 - State " + output27State + "</p>");
                        // If the output27State is off, it displays the ON button
                        if (output27State == "off")
                        {
                            client.println("<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>");
                        }
                        else
                        {
                            client.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
                        }

                        client.println("</body></html>");

                        // The HTTP response ends with another blank line
                        client.println();
                        // Break out of the while loop
                        break;
                    }
                    else
                    { // if you got a newline, then clear currentLine
                        currentLine = "";
                    }
                }
                else if (c != '\r')
                {                     // if you got anything else but a carriage return character,
                    currentLine += c; // add it to the end of the currentLine
                }
            }
        }
        // Clear the header variable
        header = "";
        // Close the connection
        client.stop();
        DBGln("Client disconnected.");
        DBGln("");
    }
    delay(1000);
}
#endif
