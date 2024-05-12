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
#define WIFI_NUMBER_OF_TRIES 15
#define WIFI_RECONNECT_TRY_IN_INTERVALL 10000

static unsigned long previousMillis = 0;
static bool helpConnect(Setup &setup);

static char *ssid = "";
static char *password = "";
static int wiFiStatus;

void ConnectedToAP_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info)
{
    DBGf("wlan::Connected to AP: %s", WiFi.SSID().c_str());
}

void GotIP_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info)
{
    // Serial.print("Local ESP32 IP: ");
    // DBGf("wlan::Local ESP32 IP:: %s", WiFi.localIP().c_str());

    Serial.println("wlan::Connected to AP: " + WiFi.localIP());
}

void WiFi_Disconnected_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info)
{
    DBGf("Disconnected From WiFi Network, attempt to recconnect ssid: %s", WiFi.SSID().c_str());
    // Attempt Re-Connection
    WiFi.begin(ssid, password);
}

static char *Get_WiFiStatus(int status)
{
    switch (status)
    {
    case WL_IDLE_STATUS:
        return "WL_IDLE_STATUS";
    case WL_SCAN_COMPLETED:
        return "WL_SCAN_COMPLETED";
    case WL_NO_SSID_AVAIL:
        return "WL_NO_SSID_AVAIL";
    case WL_CONNECT_FAILED:
        return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST:
        return "WL_CONNECTION_LOST";
    case WL_CONNECTED:
        return "WL_CONNECTED";
    case WL_DISCONNECTED:
        return "WL_DISCONNECTED";
    }
}

bool wifi_init(Setup &setup)
{
    WiFi.mode(WIFI_STA);
    int numberOfTries = WIFI_NUMBER_OF_TRIES;
    WiFi.onEvent(ConnectedToAP_Handler, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(GotIP_Handler, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    ssid = setup.ssid;
    password = setup.passwd;
    WiFi.begin(ssid, password);
    /* strcpy(setup.ssid, "Milchbehaelter");
    strcpy(setup.passwd, "47754775"); */

    // WiFi.begin("setup.ssid," setup.passwd);

    DBGf("WIFI: %s, Passwd: %s", setup.ssid, setup.passwd);
    DBGf("Connecting to WiFi ..");
    tft_printInfo("Connecting to WiFi");
    wiFiStatus = WiFi.status();
    while (wiFiStatus != WL_CONNECTED && numberOfTries-- > 0)
    {
        delay(1000);
        wiFiStatus = WiFi.status();
        Serial.println(Get_WiFiStatus(wiFiStatus));
        --numberOfTries;
    }
    if (wiFiStatus == WL_CONNECTED)
    {
        DBGf("WIFI connected ");
        return true;
    }
    else
    {
        DBGf("Cannot connect to network ssid: %s", setup.ssid);
        DBG("Now scanning networks");
        tft_printInfo("", true);
        tft_printInfo("Scanning WiFi ..");
        return false;
    }
    /*  int counter = 0;
     char buff[50];
   int WiFiStatus;  memset(buff, 0, strlen(buff));
     while (WiFi.status() != WL_CONNECTED)
     {
         DBG("%c", '.');
         delay(2000);
         ++counter;
         for (int kk = 0; kk < counter; kk++)
         {
             buff[kk] = '.';
         }
         tft_printInfo(buff, false);
         if (counter == 5)
         {
             tft_printInfo("", true);
             tft_printInfo("Scanning WiFi ..");
             break;
         }
     }

     WiFi.disconnect();
     tft_print_txt(2, "Reconnect", setup.ssid);
     DBGf("[Wifi] Connecting to %s ", setup.ssid);
     WiFi.mode(WIFI_STA);
     WiFi.begin(setup.ssid, setup.passwd, true); */

    // Will try for about 10 seconds (20x 500ms)
    // Wait for the WiFi event

    return true;
}

static bool helpConnect(Setup &setup)
{
    int numberOfTries = WIFI_NUMBER_OF_TRIES;
    wl_status_t stat = WL_IDLE_STATUS;
    wl_status_t statWifi = WiFi.status();

    bool printNewLine = true;
    char buf[200];
    strcpy(setup.currentIP, "0.0.0.0");
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
            strcpy(setup.currentIP, WiFi.localIP().toString().c_str());
            return true;
            break;
        default:
            DBG("[WiFi] WiFi Status:");
            sprintf(buf, "[%d]", /*WiFi.status(),*/ WIFI_NUMBER_OF_TRIES - numberOfTries);
            tft_printInfo(buf, printNewLine);
            break;
        }
        delay(WIFI_TRY_DELAY);

        if (numberOfTries <= 0)
        {
            DBGf("[WiFi] Failed to connect to WiFi!");

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
    return strcmp(setup.currentIP, "0.0.0.0") == 0 ? false : true;
}

void wifi_scan_network()
{

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
        char buf[50];
        char *cp = "";
        tft_printInfo("Networks: ");
        DBGf("Found %d net", n);
        for (int i = 0; i < n; ++i)
        {
            switch (WiFi.encryptionType(i))
            {
            case WIFI_AUTH_OPEN:
                cp = "open";
                break;
            case WIFI_AUTH_WEP:
                cp = "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                cp = "WPA";
                break;
            case WIFI_AUTH_WPA2_PSK:
                cp = "WPA2";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                cp = "WPA+WPA2";
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                cp = "WPA2-EAP";
                break;
            case WIFI_AUTH_WPA3_PSK:
                cp = "WPA3";
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                cp = "WPA2+WPA3";
                break;
            case WIFI_AUTH_WAPI_PSK:
                cp = "WAPI";
                break;
            default:
                cp = "unknown";
            }
            DBGf("[%d]:%s(%d) Authorisierung: %s", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), cp);
            sprintf(buf, "[%s]:%d Authorisierung: %s", WiFi.SSID(i).c_str(), WiFi.RSSI(i), cp);
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
/* bool wifi_tryToReconnect(char **action)
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
} */