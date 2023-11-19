#define _WIFI_CPP

#include <WiFi.h>
#include <tft.h>
#include "wlan.h"
#include "defines.h"

/*   DEFInES
 */
#define WIFI_RECONNECT_START "Reconnect"
#define WIFI_RECONNECT_DONE "Connected"
#define WIFI_RECONNECT_FALSE "Not Connected"

#define WIFI_TRY_DELAY 1000
#define WIFI_NUMBER_OF_TRIES 7
#define WIFI_RECONNECT_TRY_IN_INTERVALL 30000

static unsigned long previousMillis = 0;

bool wifi_init(Setup &setup)
{
#ifdef WEB
    int numberOfTries = 0;
#else
    int numberOfTries = WIFI_NUMBER_OF_TRIES;
#endif
    tft_print_txt(2, "Connect to", setup.ssid);
    DBGf("[Wifi] Connecting to %s ", setup.ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(setup.ssid, setup.passwd, true);

    // Will try for about 10 seconds (20x 500ms)
    // Wait for the WiFi event
    wl_status_t stat = WL_IDLE_STATUS;
    wl_status_t statWifi = WiFi.status();
    bool printNewLine = true;
    char buf[50];

    while (true)
    {

        switch (statWifi)
        {
        case WL_NO_SSID_AVAIL:
            DBGf("[WiFi] SSID not found: %s", setup.ssid);
            sprintf(buf, " not found [%d]", WIFI_NUMBER_OF_TRIES - numberOfTries);
            tft_printInfo(buf, printNewLine);

            break;
        case WL_CONNECT_FAILED:
            DBGf("[WiFi] Failed - WiFi not connected! Reason: ");
            sprintf(buf, "No Connection [%d]", WIFI_NUMBER_OF_TRIES - numberOfTries);
            tft_printInfo(buf, printNewLine);

            return false;
            break;
        case WL_CONNECTION_LOST:
            DBGf("[WiFi] Connection was lost");
            sprintf(buf, "Lost Connection [%d]", WIFI_NUMBER_OF_TRIES - numberOfTries);
            tft_printInfo(buf, printNewLine);

            break;
        case WL_SCAN_COMPLETED:
            DBGf("[WiFi] Scan is ready");
            sprintf(buf, "Wifi scan: Fertig [%d]", WIFI_NUMBER_OF_TRIES - numberOfTries);
            tft_printInfo(buf, printNewLine);
            break;
        case WL_DISCONNECTED:
            DBGf("[WiFi] WiFi is disconnected");

            sprintf(buf, "Disconnected[%d]", WIFI_NUMBER_OF_TRIES - numberOfTries);
            tft_printInfo(buf, printNewLine);

            break;
        case WL_CONNECTED:
            DBG("[WiFi] WiFi is connected!");
            sprintf(buf, "WiFi connected [%d]", WIFI_NUMBER_OF_TRIES - numberOfTries);
            tft_printInfo(buf, printNewLine);
            DBGf("[WiFi] IP address: %s", WiFi.localIP().toString().c_str());

            return true;
            break;
        default:
            DBG("[WiFi] WiFi Status: %x", WiFi.status());
            sprintf(buf, "%s [%d]", WiFi.status(), WIFI_NUMBER_OF_TRIES - numberOfTries);
            tft_printInfo(buf, printNewLine);
            break;
        }
        delay(WIFI_TRY_DELAY);

        if (numberOfTries <= 0)
        {
            DBGf("[WiFi] Failed to connect to WiFi! %d", WiFi.status());

            // Use disconnect function to force stop trying to connect
            WiFi.disconnect();
            return false;
        }
        else
        {
            numberOfTries--;
        }
        stat = statWifi;
        if (numberOfTries == 0)
            printNewLine = true;
        else
            printNewLine = stat == statWifi ? false : true;
        statWifi = WiFi.status();
    }
    return true;
}

void wifi_scan_network()
{

    /*
    tft_getRoot().setTextColor(TFT_GREEN, TFT_BLACK);
    tft_getRoot().fillScreen(TFT_BLACK);
    tft_getRoot().setTextDatum(MC_DATUM);
    tft_getRoot().setTextSize(3);

    tft_getRoot().drawString("Scan Network", tft_getWidth() / 2, tft_getHeight() / 2); */

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(500);
    tft_printInfo("Scan for Wlan ...");
    int16_t n = WiFi.scanNetworks();
    // tft_getRoot().fillScreen(TFT_BLACK);
    if (n == 0)
    {
        tft_printInfo("No networks found");
    }
    else
    {
        DBGf(" ======   WLAN Networks ============");
        // tft_getRoot().setTextDatum(TL_DATUM);
        // tft_getRoot().setCursor(0, 0, 4);
        char buf[30];
        tft_printInfo("Networks: ");
        DBGf("Found %d net", n);
        for (int i = 0; i < n; ++i)
        {
            DBGf("[%d]:%s(%d)", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
            sprintf(buf, "[%s]:%d", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
            tft_printInfo(buf);
            // tft_showAvailableNetworks(1, "" + (i + 1), WiFi.SSID(i).c_str(), "" + WiFi.RSSI(i));
        }
    }
    // WiFi.mode(WIFI_OFF);
    tft_printInfo(" ");
    tft_printInfo("Switch to AP-Mode!!");
    
    delay(3000);
    tft_clearScreen();
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

        DBGf("Reconnecting to WiFi...");
        WiFi.disconnect();
        WiFi.reconnect();
        previousMillis = currentMillis;
    }
    bool status = WiFi.isConnected();
    *action = (status == true) ? (char *)WIFI_RECONNECT_DONE : (char *)WIFI_RECONNECT_FALSE;
    return status;
}