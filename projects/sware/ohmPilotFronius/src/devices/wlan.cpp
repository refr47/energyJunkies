#define _WIFI_CPP

#include <WiFi.h>
#include <esp_wifi.h>
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

static void readMacAddress() // 34:85:18:8b:9d:30
{
    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK)
    {
        Serial.printf("\nMac Adresse: %02x:%02x:%02x:%02x:%02x:%02x\n",
                      baseMac[0], baseMac[1], baseMac[2],
                      baseMac[3], baseMac[4], baseMac[5]);
    }
    else
    {
        Serial.println("wlan::readMacAddress() Failed to read MAC address");
    }
}

void ConnectedToAP_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info)
{
    LOG_INFO("wlan::(ConnectedToAP_Handler) Connected to AP, haven't got IP yet, ssid: %s", WiFi.SSID().c_str());
    connected = false;
}

// only called if status == WL_CONNECTED & password is ok & ap has admitted connection
void GotIP_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info)
{
    String s = WiFi.localIP().toString();
    Serial.print("\nLocal ESP32 IP: ");
    Serial.print(s.c_str());
    Serial.print("\n");
    LOG_INFO("wlan::(GotIP_Handler) Connected to AP:: %s", s.c_str());
    connected = true;
    // Serial.println("wlan::Connected to AP: ");
}

void WiFi_Disconnected_Handler(WiFiEvent_t wifi_event, WiFiEventInfo_t wifi_info)
{
    LOG_INFO("wlan::(WiFi_Disconnected_Handler) Disconnected From WiFi Network, attempt to recconnect ssid: %s, ssid: %s, Password: %s", WiFi.SSID().c_str(), ssid, password);
    // Attempt Re-Connection
    connected = false;
    WiFi.begin(ssid, password);
    delay(1000);
    wiFiStatus = WiFi.status();
    LOG_INFO("wlan::Reconnect, status: %s", Get_WiFiStatus(wiFiStatus));
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
    LOG_INFO("wifi_init()");
    DBG("wlan::wifi_init()");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    readMacAddress();
    delay(1000);
    // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(NET_HOSTNAME); // define hostname defines.h
    int numberOfTries = WIFI_NUMBER_OF_TRIES;
    WiFi.onEvent(ConnectedToAP_Handler, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(GotIP_Handler, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WiFi_Disconnected_Handler, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    ssid = setup.ssid;
    password = setup.passwd;
    connected = false;
    WiFi.begin(ssid, password);
    LOG_DEBUG("WIFI: %s, Passwd: %s", setup.ssid, setup.passwd);
    LOG_DEBUG("Connecting to WiFi ..");
    tft_printInfo("Connecting to WiFi");
    delay(1000);
    wiFiStatus = WiFi.status();
    DBGf("wlan::connect - status: %d", wiFiStatus);

    while (wiFiStatus != WL_CONNECTED && numberOfTries-- > 0 && !connected)
    {
        delay(1000);
        wiFiStatus = WiFi.status();
        Serial.println(Get_WiFiStatus(wiFiStatus));
        --numberOfTries;
        LOG_DEBUG("wlan::Not connectetd, tries left: %d", numberOfTries);
        DBGf("wlan::Not connectetd, tries left: %d", numberOfTries);
    }
    if (wiFiStatus == WL_CONNECTED && connected == true)
    {
        LOG_INFO("wlan::WIFI connected ");

        String s = WiFi.localIP().toString();
        Serial.print(WiFi.localIP());
        DBGf("wlan::WIFI connected with IP: %s ", s.c_str());

        strcpy(setup.currentIP, WiFi.localIP().toString().c_str());
        return true;
    }
    else
    {
        LOG_ERROR("Cannot connect to network ssid: %s", setup.ssid);
        LOG_DEBUG("Now scanning networks");
        tft_printInfo("", true);
        tft_printInfo("Scanning WiFi ..");
        return false;
    }

    return wiFiStatus == WL_CONNECTED;
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
        LOG_INFO(" ======   WLAN Networks ============");
        // tft_getRoot().setTextDatum(TL_DATUM);
        // tft_getRoot().setCursor(0, 0, 4);
        char buf[50];
        char *cp = "";
        tft_printInfo("Networks: ");
        LOG_INFO("Found %d net", n);
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
            LOG_INFO("[%d]:%s(%d) Authorisierung: %s", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), cp);
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
    localIP.toString().toCharArray(*pBuffer16, INET_ADDRSTRLEN);
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

        WiFi.disconnect();
        delay(3000);
        connected = false;
        WiFi.begin(ssid, password);
        Serial.println(Get_WiFiStatus(wiFiStatus));
        --numberOfTries;
        delay(7000);
        wiFiStatus = WiFi.status();
        LOG_INFO("wlan::reconnect Not connectetd, tries left: %d", numberOfTries);
    }
    if (wiFiStatus == WL_CONNECTED && connected == true)
    {
        LOG_INFO("wlan::reconnect - WIFI connected ");
        strcpy(setup.currentIP, WiFi.localIP().toString().c_str());
        return true;
    }
    else
    {
        WiFi.disconnect();
        LOG_INFO("wlan:: Reconnect - Cannot connect to network ssid: %s", setup.ssid);
        LOG_INFO("wlan:: Reconnect Now scanning networks");
        wifi_scan_network();
        return false;
    }
}
