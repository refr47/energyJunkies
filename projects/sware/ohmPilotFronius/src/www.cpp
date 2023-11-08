#define __ACCESS_POINT_CPP

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <mdns.h>
#include <AsyncJson.h>

#include "tft.h"
#include "eprom.h"
#include "utils.h"
#include "www.h"

// WiFiServer server(80);
const char *PARAM_INPUT_1 = "passwd";
const char *PARAM_INPUT_2 = "state";

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
 */
static void handleStoreSetup(AsyncWebServerRequest *request, JsonVariant &json);
static void sendHTML(String &path, AsyncWebServerRequest *request);
static void listDir(char *dir);

/*
 ***********************  local variables
 */

static AsyncWebServer server(80);

// Flag to check if the user is authenticated
static bool isAuthenticated = false;

static void listDir(char *dir)
{
    DBGln("listdir");
    File root = SPIFFS.open(dir);

    if (!root)
    {
        DBGln("- failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        DBGln(" - not a directory");
        return;
    }

    DBGln(root);
    File file = root.openNextFile();
    DBGln(file);
    while (file)
    {

        DBG("FILE: ");
        DBGln(file.name());

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
    DBGln("handleLogin");
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
            DBG("Stammdaten: ");
            DBGln(standingData);

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
    DBGln("handleRoot");
    if (isAuthenticated)
        request->send(SPIFFS, "/index.html", String(), false);
    else
        request->send(SPIFFS, "/login.html", "text/html", false);
}

static void handleSetup(AsyncWebServerRequest *request)
{
    // Check if the user is authenticated
    DBGln("handleSetup");
    if (isAuthenticated)
        request->send(SPIFFS, "/setupData.html", "text/html", false);
    else
        request->send(SPIFFS, "/login.html", "text/html", false);
}

static void returnFromStoreSetup(bool inputCorrect, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, AsyncWebServerRequest *request)
{
    String response;
    if (inputCorrect)
    {
        data["done"] = 1;
        data["error"] = "";
        DBGln("returnFromStoreSetup - no errors ");
        // eprom_storeSetup(setup);
    }
    else
    {
        data["done"] = 0;
        DBGln("returnFromStoreSetup -  errors ");
    }

    DBGln(" vor serialisierung : ");
    serializeJson(data, response);
    DBG("storeSetUp - return: ");
    DBGln(response);

    request->send(200, "application/json", response);
}

static void dbg(char *key, const char *val)
{
    DBG("ARG: ");
    DBG(key);
    DBG(" VAL: ");
    DBGln(val);
}

static void handleGetSetup(AsyncWebServerRequest *request)
{
    StaticJsonDocument<JSON_OBJECT_SETUP_LEN> data;

    return returnFromStoreSetup(true, data, request);
}

static void handleStoreSetup(AsyncWebServerRequest *request, JsonVariant &json)
{
    const JsonObject &jsonObj = json.as<JsonObject>();
    StaticJsonDocument<JSON_OBJECT_SETUP_LEN> data;

    for (JsonPair keyValue : jsonObj)
    {
        Serial.print(keyValue.key().c_str());
    }
    char *cP = NULL;
    Setup setup; // eprom write
    const char *argument = jsonObj[WLAN_ESSID];
    int result = 0;
    float resultF = 0.0;
    bool errorH = false;
    DBGln("STart parsing json object");
    dbg(WLAN_ESSID, argument);
    errorH = util_isFieldFilled(WLAN_ESSID, argument, data);
    if (errorH)
        strcpy(setup.ssid, jsonObj[WLAN_ESSID]);
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[WLAN_PASSWD];
    dbg(WLAN_PASSWD, argument);
    errorH = util_isFieldFilled(WLAN_PASSWD, argument, data);
    if (errorH)
        strcpy(setup.passwd, jsonObj[WLAN_PASSWD]);
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[IP_INVERTER];
    dbg(IP_INVERTER, argument);
    errorH = util_isFieldFilled(IP_INVERTER, argument, data);
    if (errorH)
    {
        bool result = true;
        String ipInv = argument;
        if (!result)
        {
            DBG("handleStoreSetup IP translate did not succeed for IP: ");
            DBGln(argument);
            data["error"] = "IP-Transformation war nicht erfolgreich!";
            return returnFromStoreSetup(false, data, request);
        }
        else
            setup.ipInverter = ipv4_string_to_int(ipInv, &result);
    }
    else
        return returnFromStoreSetup(errorH, data, request);

    argument = jsonObj[HYSTERESE];
    dbg(HYSTERESE, argument);
    errorH = util_checkParamInt(HYSTERESE, argument, data, &result);
    if (errorH)
        setup.regelbereichHysterese = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[EINSPEISUNG_MUSS];
    dbg(EINSPEISUNG_MUSS, argument);
    errorH = util_checkParamInt(EINSPEISUNG_MUSS, argument, data, &result);
    if (errorH)
        setup.pid_targetPowerInWatt = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[MINDEST_LAUFZEIT_DIGITALER_OUT];
    dbg(MINDEST_LAUFZEIT_DIGITALER_OUT, argument);

    errorH = util_checkParamInt(MINDEST_LAUFZEIT_DIGITALER_OUT, argument, data, &result);
    if (errorH)
        setup.pid_min_time_before_switch_off_channel_inMS = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[MINDEST_LAUFZEIT_PORT_ON];
    dbg(MINDEST_LAUFZEIT_PORT_ON, argument);
    errorH = util_checkParamInt(MINDEST_LAUFZEIT_PORT_ON, argument, data, &result);
    if (errorH)
        setup.pid_min_time_for_dig_output_inMS = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[MINDEST_LAUFZEIT_REGLER_KONSTANT];
    dbg(MINDEST_LAUFZEIT_REGLER_KONSTANT, argument);
    errorH = util_checkParamInt(MINDEST_LAUFZEIT_REGLER_KONSTANT, argument, data, &result);
    if (errorH)
        setup.pid_min_time_without_contoller_inMS = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[EXTERNER_SPEICHER];
    dbg(EXTERNER_SPEICHER, argument);
    if (util_isFieldFilled(EXTERNER_SPEICHER, argument, data))
    {
        setup.externerSpeicher = *argument == 'j' ? true : false;
    }
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[EXTERNER_SPEICHER_PRIORI];
    dbg(EXTERNER_SPEICHER_PRIORI, argument);
    if (util_isFieldFilled(EXTERNER_SPEICHER_PRIORI, argument, data))
    {
        setup.externerSpeicherPriori = *argument; // j1|2|3
    }
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[AUSSCHALT_TEMP];
    dbg(AUSSCHALT_TEMP, argument);
    errorH = util_checkParamInt(AUSSCHALT_TEMP, argument, data, &result);
    if (errorH)
        setup.ausschaltTempInGradCel = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[PID_P];
    dbg(PID_P, argument);
    errorH = util_checkParamFloat(PID_P, argument, data, &resultF);
    if (errorH)
        setup.pid_p = resultF;
    argument = jsonObj[PID_I];
    dbg(PID_I, argument);

    errorH = util_checkParamFloat(PID_I, argument, data, &resultF);
    if (errorH)
        setup.pid_i = resultF;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[PID_D];
    dbg(PID_D, argument);
    errorH = util_checkParamFloat(PID_D, argument, data, &resultF);
    if (errorH)
        setup.pid_d = resultF;
    else
        return returnFromStoreSetup(errorH, data, request);

    eprom_storeSetup(setup);
    eprom_test_read_Eprom();
    return returnFromStoreSetup(errorH, data, request);
    // esp_restart();
}

void www_init(char *ipAddr, char *wlanAsClientSSID)
{
    // Initialize SPIFFS

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        DBGln("SPIFFS Mount Failed");
        return;
    }

    // listDir("/");
    if (ipAddr == NULL)
    {
        //  Connect to Wi-Fi network with SSID_FOR_ACCESS_POINT
        DBG("Setting AP (Access Point)…");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(SSID_FOR_ACCESS_POINT);
        IPAddress IP = WiFi.softAPIP();
        DBG("AP IP address: ");
        ipAddr = (char *)IP.toString().c_str();
        DBG(IP.toString());
        DBGln(ipAddr);
        tft_initNetwork(6, "Netzwerkparameter", "ACCESS-Point Modus", "ssid=>", SSID_FOR_ACCESS_POINT, "IP=>", IP.toString().c_str());
    }
    else
    {
        DBG("WWW init server with ip: ");
        DBGln(ipAddr);
        tft_initNetwork(6, "Netzwerkparameter", "Client", "ssid=>", wlanAsClientSSID, "IP=>", ipAddr);
    }

    server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/about.html", "text/html", false, NULL); });
    server.on("/headerF.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/headerF.html", "text/html"); });
    server.on("/output.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/output.html", "text/html"); });
    server.on("/logs.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/logs.html", "text/html"); });
    // Route for serving the login page and handling login requests
    server.on("/login", HTTP_POST, handleLogin);

    // Route for serving the root page  request->send(SPIFFS, "/index.html", String(), false, callBack);
    server.on("/", HTTP_GET, handleRoot);
    server.on("/setup", HTTP_GET, handleSetup);
    server.on("/getSetup", HTTP_GET, handleGetSetup);

    // Route to load style.css file
    server.on("/css/main.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/css/main.css", "text/css"); });
    server.on("/css/menu.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/css/menu.css", "text/css"); });
    server.on("/css/stammdaten.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/css/stammdaten.css", "text/css"); });
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
                                                                           { handleStoreSetup(request, json); });

    // Start the server
    server.addHandler(handler);
    // Route for serving static files from SPIFFS
    server.onNotFound([](AsyncWebServerRequest *request)
                      {

    String path = request->url();
    DBG("Path (!found): ");
    DBG(path.c_str());
    DBGln(";");
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
    } });
    server.begin();
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
