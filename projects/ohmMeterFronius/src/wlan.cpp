#define _WIFI_CPP

#include <WiFi.h>
#include "wlan.h"

/*   DEFInES
 */

#define WIFI_TRY_DELAY 500
#define WIFI_NUMBER_OF_TRIES 20
#define WIFI_RECONNECT_TRY_IN_INTERVALL 30000

/* #define SSID "WLAN-HTLW"
#define PASSWD "HTL-Wels" */

#define SSID "BART_LOW"
#define PASSWD "47754775"

bool wifi_init()
{
    int numberOfTries = WIFI_NUMBER_OF_TRIES;

    Serial.println();
    Serial.print("[Wifi] Connecting to ");
    Serial.println(SSID);
    WiFi.begin(SSID, PASSWD);
    // Will try for about 10 seconds (20x 500ms)
    // Wait for the WiFi event

    while (true)
    {

        switch (WiFi.status())
        {
        case WL_NO_SSID_AVAIL:
            Serial.println("[WiFi] SSID not found");
            break;
        case WL_CONNECT_FAILED:
            Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
            return false;
            break;
        case WL_CONNECTION_LOST:
            Serial.println("[WiFi] Connection was lost");
            break;
        case WL_SCAN_COMPLETED:
            Serial.println("[WiFi] Scan is completed");
            break;
        case WL_DISCONNECTED:
            Serial.println("[WiFi] WiFi is disconnected");
            break;
        case WL_CONNECTED:
            Serial.println("[WiFi] WiFi is connected!");
            Serial.print("[WiFi] IP address: ");
            Serial.println(WiFi.localIP());
            return true;
            break;
        default:
            Serial.print("[WiFi] WiFi Status: ");
            Serial.println(WiFi.status());
            break;
        }
        delay(WIFI_TRY_DELAY);

        if (numberOfTries <= 0)
        {
            Serial.print("[WiFi] Failed to connect to WiFi!");
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
    unsigned long previousMillis = 0;
    unsigned long interval = WIFI_RECONNECT_TRY_IN_INTERVALL;
    // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
    *action = (char *)WIFI_RECONNECT_START;

    if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
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