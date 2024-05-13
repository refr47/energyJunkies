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

// static unsigned long previousMillis = 0;
static char *Get_WiFiStatus(int status);

static char *ssid = "";
static char *password = "";
static int wiFiStatus;
static bool connected = false;

void ConnectedToAP_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info)
{
    DBGf("wlan::(ConnectedToAP_Handler) Connected to AP, haven't got IP yet, ssid: %s", WiFi.SSID().c_str());
    connected = false;
}

// only called if status == WL_CONNECTED & password is ok & ap has admitted connection
void GotIP_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info)
{
    // Serial.print("Local ESP32 IP: ");
    DBGf("wlan::(GotIP_Handler) Connected to AP:: %s", WiFi.localIP().toString().c_str());
    connected = true;
    Serial.println("wlan::Connected to AP: " + WiFi.localIP());
}
void WiFi_Disconnected_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info)
{
    DBGf("wlan::(WiFi_Disconnected_Handler) Disconnected From WiFi Network, attempt to recconnect ssid: %s, ssid: %s, Password: %s", WiFi.SSID().c_str(), ssid, password);
    // Attempt Re-Connection
    connected = false;
    WiFi.begin(ssid, password);
    delay(1000);
    wiFiStatus = WiFi.status();
    DBGf("wlan::Reconnect, status: %s", Get_WiFiStatus(wiFiStatus));
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
    WiFi.onEvent(WiFi_Disconnected_Handler, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    ssid = setup.ssid;
    password = setup.passwd;
    WiFi.begin(ssid, password);
    DBGf("WIFI: %s, Passwd: %s", setup.ssid, setup.passwd);
    DBGf("Connecting to WiFi ..");
    tft_printInfo("Connecting to WiFi");
    delay(1000);
    wiFiStatus = WiFi.status();
    while (wiFiStatus != WL_CONNECTED && numberOfTries-- > 0 && !connected)
    {
        delay(1000);
        wiFiStatus = WiFi.status();
        Serial.println(Get_WiFiStatus(wiFiStatus));
        --numberOfTries;
        DBGf("wlan::Not connectetd, tries left: %d", numberOfTries);
    }
    if (wiFiStatus == WL_CONNECTED && connected == true)
    {
        DBGf("wlan::WIFI connected ");
        strcpy(setup.currentIP, WiFi.localIP().toString().c_str());
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

    return true;
}

void wifi_scan_network()
{

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(500);
    tft_printInfo("Scan for Wlan ...");
    int16_t n = WiFi.scanNetworks();

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

bool wifi_isStillConnected(Setup &setup)
{
    // DBGf("wlan::wifi_isStillConnected, connected: %d", connected);
    wiFiStatus = WiFi.status();
    if (connected)
    {

        if (wiFiStatus == WL_CONNECTED)
        {
            return true;
        }
    }

    int numberOfTries = WIFI_NUMBER_OF_TRIES;
    while (((wiFiStatus != WL_CONNECTED) || connected == false) && numberOfTries-- > 0)
    {
        delay(1000);
        wiFiStatus = WiFi.status();
        Serial.println(Get_WiFiStatus(wiFiStatus));
        --numberOfTries;
        DBGf("wlan::reconnect Not connectetd, tries left: %d", numberOfTries);
    }
    if (wiFiStatus == WL_CONNECTED && connected == true)
    {
        DBGf("wlan::reconnect - WIFI connected ");
        strcpy(setup.currentIP, WiFi.localIP().toString().c_str());
        return true;
    }
    else
    {
        WiFi.disconnect();
        DBGf("wlan:: Reconnect - Cannot connect to network ssid: %s", setup.ssid);
        DBG("wlan:: Reconnect Now scanning networks");
        wifi_scan_network();
        return false;
    }
}
