#define __ACCESS_POINT_CPP

#include <WiFi.h>
#include <SPIFFS.h>
#include <mdns.h>

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
    // Initialize SPIFFS
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        LOG_ERROR("www_init::SPIFFS Mount Failed");
        tft_printKeyValue("Init Flash File", "Error", TFT_RED);
        tft_printKeyValue("Cannot Start WebServer !!", "Error", TFT_RED);
        return false;
    }
    tft_printKeyValue("Init Flash File", "OK", TFT_GREEN);
    // listDir("/");
    if (ipAddr == NULL)
    {
        //  Connect to Wi-Fi network with SSID_FOR_ACCESS_POINT
        LOG_INFO("www_init::Setting AP (Access Point) mode");
        isAPModus = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(SSID_FOR_ACCESS_POINT, DEFAULT_IP_ACCESS_POINT);

        ipAddr = DEFAULT_IP_ACCESS_POINT;
        strcpy(setupData.currentIP, ipAddr);
        LOG_INFO("www_init::AP IP address: %s", ipAddr);

        tft_printKeyValue("ACCESS Point", "OK", TFT_GREEN);
        tft_printKeyValue("SSID", SSID_FOR_ACCESS_POINT, TFT_GREEN);
        tft_printKeyValue("IP", DEFAULT_IP_ACCESS_POINT, TFT_GREEN);
    }
    else
    {
        LOG_INFO("www_init::Start Webserver with ip: %s", ipAddr);
        isAPModus = false;
        // tft_printInfo("Start WWW on:");

        tft_printKeyValue("SSID", wlanAsClientSSID, TFT_GREEN);
        tft_printKeyValue("IP", ipAddr, TFT_GREEN);
    }

    server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/about.html", "text/html", false, NULL); });
    server.on("/shelly", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/shelly.html", "text/html", false, NULL); });
    server.on("/headerF.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/headerF.html", "text/html"); });
    server.on("/out", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/output.html", "text/html"); });
    /* server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", "<!DOCTYPE html><html><head><title>Serial Monitor</title><script>function fetchData(){fetch('/serial').then(response => response.text()).then(data => {document.getElementById('output').innerText = data;});} setInterval(fetchData, 1000);</script></head><body><h1>ESP32 Serial Monitor</h1><pre id='output'></pre></body></html>"); });
 */
    server.on("/serial", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", logReader_getBufferAsString()); });
    server.on("/logOn", HTTP_GET, [](AsyncWebServerRequest *request)
              { logReader_enDisableRedirect(true);
                request->send(200, "text/plain", "Redirecting enabled"); });
    server.on("/logOff", HTTP_GET, [](AsyncWebServerRequest *request)
              { logReader_enDisableRedirect(false);
                request->send(200, "text/plain", "Redirecting disabled"); });

    server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request)
              { logReader_enDisableRedirect(true);
                    request->send(SPIFFS, "/logs.html", "text/html"); });

    server.on("/setup", HTTP_GET, handleSetup);
    server.on("/", HTTP_GET, handleRoot);
    // Route for serving the login page and handling login requests
    server.on("/login", HTTP_POST, handleLogin);

    // Route for serving the root page  request->send(SPIFFS, "/index.html", String(), false, callBack);

    server.on("/getSetup", HTTP_GET, ajaxCalls_handleGetSetup);
    server.on("/getOverview", HTTP_GET, ajaxCalls_handleGetOverview);

    // Route to load style.css file
    server.on("/css/main.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/css/main.css", "text/css"); });
    server.on("/css/menu.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/css/menu.css", "text/css"); });
    server.on("/css/stammdaten.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/css/stammdaten.css", "text/css"); });
    server.on("/css/shelly.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/css/shelly.css", "text/css"); });
    server.on("/css/jquery-3.7.1.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/css/jquery-3.7.1.min.css", "text/css"); });
    server.on("/css/datatables.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/css/datatables.min.css", "text/css"); });

    server.on("/js/jquery-3.7.1.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/js/jquery-3.7.1.min.js", String()); });
    server.on("/js/stammdaten.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/js/stammdaten.js", String()); });
    server.on("/js/navigation.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/js/navigation.js", String()); });
    server.on("/js/shelly.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/js/shelly.js", String()); });
    server.on("/js/datatables.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/js/datatables.min.js", String()); });

    server.on("/img/icon.jpg", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/img/icon.jpg", "image/jpg"); });
    server.on("/img/Energies.jpg", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/img/Energies.jpg", "image/jpg"); });
    server.on("/img/harvester.jpg", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/img/harvester.jpg", "image/jpg"); });

    // https://github.com/me-no-dev/ESPAsyncWebServer

    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/storeSetup", [](AsyncWebServerRequest *request, JsonVariant &json)
                                                                           { ajaxCalls_handleStoreSetup(request, json, isAPModus); });

    // Start the server
    server.addHandler(handler);
    // Route for serving static files from SPIFFS
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
        if (request->method() == HTTP_OPTIONS)
        {
            request->send(200);
        } else {
            String path = request->url();
            LOG_ERROR("www_init::Path  %s !found ", path.c_str());

            if (!isAuthenticated) {
            // Redirect to the login page if not authenticated
            request->redirect("/login");
            return;
            }

            if (SPIFFS.exists(path)) {
            AsyncWebServerResponse* response = request->beginResponse(SPIFFS, path, String(), true);
            response->addHeader("Cache-Control", "max-age=600");
            request->send(response);
            } else {
            request->send(404, "text/plain", "File not found");
            }
        } });
    tft_printKeyValue("Start WWW", "Done", TFT_GREEN);
#ifdef CORS_DEBUG
    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), F("*"));
    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), F("content-type"));
#endif
    ajaxCalls_init(webSockData, setupChanged);
    server.addHandler(webSockets_init(webSockData));
    server.begin();
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
