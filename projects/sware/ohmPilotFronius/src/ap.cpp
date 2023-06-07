#define __ACCESS_POINT_CPP
#include "tft.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// WiFiServer server(80);
const char *PARAM_INPUT_1 = "passwd";
const char *PARAM_INPUT_2 = "state";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

const char *ssid = "EnergieJunkies";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>EnergieJunkies Initialisierung</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
    input:checked+.slider {background-color: #b30000}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>EnergieJunkies Web Server</h2>
  %BUTTONPLACEHOLDER%
<script>function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?output="+element.id+"&state=0", true); }
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

String outputState(int output)
{
    if (digitalRead(output))
    {
        return "checked";
    }
    else
    {
        return "";
    }
}
// Replaces placeholder with button section in your web page
String processor(const String &var)
{
    // Serial.println(var);
    if (var == "BUTTONPLACEHOLDER3")
    {
        String buttons = "";
        buttons += "<h4>Output - GPIO 2</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"2\" " + outputState(2) + "><span class=\"slider\"></span></label>";
        buttons += "<h4>Output - GPIO 4</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"4\" " + outputState(4) + "><span class=\"slider\"></span></label>";
        buttons += "<h4>Output - GPIO 33</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"33\" " + outputState(33) + "><span class=\"slider\"></span></label>";
        return buttons;
    }

    if (var == "BUTTONPLACEHOLDER")
    {
        String form = "";
        form += "<form action=\"/get\"> Password: <input type=\"text\" name=\"passwd\">";
        form += "<input type = \" submit \" value = \" Submit \"> </form><br>";
        return form;
    }

    return String();
}

void ap_init()
{

    // Connect to Wi-Fi network with SSID
    Serial.print("Setting AP (Access Point)…");
    WiFi.softAP(ssid);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    // char *p = (char *)IP.toString().c_str();
    Serial.println(IP);
    tft_initNetwork(6, "Keine Netzwerkparameter", "ACCESS-Point Modus", "SSID=>", ssid, "IP=>", (char *)IP.toString().c_str());

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", index_html, processor); });

    // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    server.on("/GET", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    String inputMessage1;
    
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) ) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
     
      //digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
        Serial.print("GET param: ");
        Serial.println(inputMessage1);
    }
    else {
      inputMessage1 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    request->send(200, "text/plain", "OK"); });
    server.begin();
}

void ap_run()
{
}

#ifdef III
void ap_run()
{
    WiFiClient client = server.available(); // Listen for incoming clients

    if (client)
    {                                  // If a new client connects,
        Serial.println("New Client."); // print a message out in the serial port
        String currentLine = "";       // make a String to hold incoming data from the client
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
                            Serial.println("GPIO 26 on");
                        }
                        else if (header.indexOf("GET /26/off") >= 0)
                        {
                            Serial.println("GPIO 26 off");
                            output26State = "off";
                        }
                        else if (header.indexOf("GET /27/on") >= 0)
                        {
                            Serial.println("GPIO 27 on");
                        }
                        else if (header.indexOf("GET /27/off") >= 0)
                        {
                            Serial.println("GPIO 27 off");
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
        Serial.println("Client disconnected.");
        Serial.println("");
    }
    delay(1000);
}
#endif

#ifdef III
/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-http-request
 */

#endif