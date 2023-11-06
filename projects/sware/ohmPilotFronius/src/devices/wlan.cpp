#define _WIFI_CPP

#include <WiFi.h>
#include "wlan.h"
#include "defines.h"
#include "tft.h"

/*   DEFInES
 */
#define WIFI_RECONNECT_START "Reconnect"
#define WIFI_RECONNECT_DONE "Connected"
#define WIFI_RECONNECT_FALSE "Not Connected"

#define WIFI_TRY_DELAY 1000
#define WIFI_NUMBER_OF_TRIES 30
#define WIFI_RECONNECT_TRY_IN_INTERVALL 30000

static unsigned long previousMillis = 0;

bool wifi_init(Setup &setup)
{
    int numberOfTries = WIFI_NUMBER_OF_TRIES;

    DBGln();

    tft_initNetwork(2, "Connect to", setup.ssid);
    DBG("[Wifi] Connecting to ");
    DBGln(setup.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(setup.ssid, setup.passwd, true);

    // Will try for about 10 seconds (20x 500ms)
    // Wait for the WiFi event

    while (true)
    {

        switch (WiFi.status())
        {
        case WL_NO_SSID_AVAIL:
            DBG("[WiFi] SSID not found: ");
            DBGln(setup.ssid);
            tft_initNetwork(3, "Connect to", setup.ssid, "SSID not found");
            break; 
        case WL_CONNECT_FAILED:
            DBG("[WiFi] Failed - WiFi not connected! Reason: ");
            tft_initNetwork(3, "Connect to", setup.ssid, "No connection");
            return false;
            break;
        case WL_CONNECTION_LOST:
            DBGln("[WiFi] Connection was lost");
            tft_initNetwork(3, "Connect to", setup.ssid, "Connection lost");
            break;
        case WL_SCAN_COMPLETED:
            DBGln("[WiFi] Scan is completed");
            break;
        case WL_DISCONNECTED:
            DBGln("[WiFi] WiFi is disconnected");
            tft_initNetwork(3, "Connect to", setup.ssid, "Disconnected");
            break;
        case WL_CONNECTED:
            DBGln("[WiFi] WiFi is connected!");
            tft_initNetwork(3, "Connect to", setup.ssid, "Connected!");
            DBG("[WiFi] IP address: ");
            DBGln(WiFi.localIP());
            return true;
            break;
        default:
            DBG("[WiFi] WiFi Status: ");
            DBGln(WiFi.status());
            tft_initNetwork(3, "Connect to", setup.ssid, (char *)WiFi.status());
            break;
        }
        delay(WIFI_TRY_DELAY);

        if (numberOfTries <= 0)
        {
            DBG("[WiFi] Failed to connect to WiFi!");
            DBGln(WiFi.status());
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
    char buff[512]; /*
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
        DBGln(" ======   WLAN Networks ============");
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
            DBGln(buff);
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
        DBG(millis());
        DBGln("Reconnecting to WiFi...");
        WiFi.disconnect();
        WiFi.reconnect();
        previousMillis = currentMillis;
    }
    bool status = WiFi.isConnected();
    *action = (status == true) ? (char *)WIFI_RECONNECT_DONE : (char *)WIFI_RECONNECT_FALSE;
    return status;
}