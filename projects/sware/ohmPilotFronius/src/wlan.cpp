#define _WIFI_CPP

#include <WiFi.h>
#include "wlan.h"
#include "defines.h"
#include "tft.h"

/*   DEFInES
 */

#define WIFI_TRY_DELAY 1000
#define WIFI_NUMBER_OF_TRIES 20
#define WIFI_RECONNECT_TRY_IN_INTERVALL 30000

unsigned long previousMillis = 0;

bool wifi_init()
{
    int numberOfTries = WIFI_NUMBER_OF_TRIES;

    Serial.println();

    tft_initNetwork(2, "Connect to", MY_SSID);
    Serial.print("[Wifi] Connecting to ");
    Serial.println(MY_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(MY_SSID, MY_PASSWD, true);

    // Will try for about 10 seconds (20x 500ms)
    // Wait for the WiFi event

    while (true)
    {

        switch (WiFi.status())
        {
        case WL_NO_SSID_AVAIL:
            Serial.println("[WiFi] SSID not found");
            tft_initNetwork(3, "Connect to", MY_SSID, "SSID not found");
            break;
        case WL_CONNECT_FAILED:
            Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
            tft_initNetwork(3, "Connect to", MY_SSID, "No connection");
            return false;
            break;
        case WL_CONNECTION_LOST:
            Serial.println("[WiFi] Connection was lost");
            tft_initNetwork(3, "Connect to", MY_SSID, "Connection lost");
            break;
        case WL_SCAN_COMPLETED:
            Serial.println("[WiFi] Scan is completed");
            break;
        case WL_DISCONNECTED:
            Serial.println("[WiFi] WiFi is disconnected");
            tft_initNetwork(3, "Connect to", MY_SSID, "Disconnected");
            break;
        case WL_CONNECTED:
            Serial.println("[WiFi] WiFi is connected!");
            tft_initNetwork(3, "Connect to", MY_SSID, "Connected!");
            Serial.print("[WiFi] IP address: ");
            Serial.println(WiFi.localIP());
            return true;
            break;
        default:
            Serial.print("[WiFi] WiFi Status: ");
            Serial.println(WiFi.status());
            tft_initNetwork(3, "Connect to", MY_SSID, (char *)WiFi.status());
            break;
        }
        delay(WIFI_TRY_DELAY);

        if (numberOfTries <= 0)
        {
            Serial.print("[WiFi] Failed to connect to WiFi!");
            Serial.println(WiFi.status());
            // Use disconnect function to force stop trying to connect
            WiFi.disconnect();
            return false;
        }
        else
        {
            numberOfTries--;
        }
    }
    return true;
}

void wifi_scan_network()
{
    char buff[512];/* 
    tft_getRoot().setTextColor(TFT_GREEN, TFT_BLACK);
    tft_getRoot().fillScreen(TFT_BLACK);
    tft_getRoot().setTextDatum(MC_DATUM);
    tft_getRoot().setTextSize(3);

    tft_getRoot().drawString("Scan Network", tft_getWidth() / 2, tft_getHeight() / 2); */

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(500);
   
    int16_t n = WiFi.scanNetworks();
    tft_getRoot().fillScreen(TFT_BLACK);
    if (n == 0)
    {
        tft_showAvailableNetworks(1, "No networks found");
    }
    else
    {
        Serial.println(" ======   WLAN Networks ============");
        tft_getRoot().setTextDatum(TL_DATUM);
        tft_getRoot().setCursor(0, 0, 4);
        Serial.printf("Found %d net\n", n);
        for (int i = 0; i < n; ++i)
        {
             sprintf(buff,
                    "[%d]:%s(%d)",
                    i + 1,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i)); 
             Serial.println(buff);
            tft_showAvailableNetworks(1, "" + (i + 1), WiFi.SSID(i).c_str(), "" + WiFi.RSSI(i));
           
        }
    }
    // WiFi.mode(WIFI_OFF);
    delay(3000);
}

void wifi_getLocalIP(char **pBuffer16)
{
    IPAddress localIP = WiFi.localIP();
    localIP.toString().toCharArray(*pBuffer16, 16);
}

bool wifi_isStillConnected()
{
    return WiFi.isConnected();
}
// inform about current state
bool wifi_tryToReconnect(char **action)
{
    unsigned long currentMillis = millis();

    // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
    *action = (char *)WIFI_RECONNECT_START;

    if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= WIFI_RECONNECT_TRY_IN_INTERVALL))
    {
        Serial.print(millis());
        Serial.println("Reconnecting to WiFi...");
        WiFi.disconnect();
        WiFi.reconnect();
        previousMillis = currentMillis;
    }
    bool status = WiFi.isConnected();
    *action = (status == true) ? (char *)WIFI_RECONNECT_DONE : (char *)WIFI_RECONNECT_FALSE;
    return status;
}