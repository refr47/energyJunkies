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

// #define WIFI_TRY_DELAY 1000
#define WIFI_NUMBER_OF_TRIES 15
// #define WIFI_RECONNECT_TRY_IN_INTERVALL 10000

// static unsigned long previousMillis = 0;
static char *Get_WiFiStatus(int status);
static void WiFiEvent(WiFiEvent_t event);
static void handleWiFiReconnect();

static char *ssid = "";
static char *password = "";
static int wiFiStatus;
static bool connected = false;
static int reconnectAttempts = 0;

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

void WiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case SYSTEM_EVENT_STA_CONNECTED:
        LOG_INFO("wlan::WiFiEvent(SYSTEM_EVENT_STA_CONNECTED) - Connected to WiFi");
        connected = false;
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:

        if (WiFi.status() != WL_CONNECTED)
        {

            LOG_INFO("wlan::WiFiEvent(SYSTEM_EVENT_STA_DISCONNECTED)- WiFi.status() != WL_CONNECTED - status: %s", Get_WiFiStatus((int)WiFi.status()));
            connected = false;
        }
        else
        {
            LOG_INFO("wlan::WiFiEvent(SYSTEM_EVENT_STA_DISCONNECTED)- event Disconnected from WiFi, but wiFi.status is wl_connected");
        }

        // wifi_scan_network();

        // handleWiFiReconnect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
    {
        String s = WiFi.localIP().toString();
        LOG_INFO("wlan::WiFiEvent(SYSTEM_EVENT_STA_GOT_IP) - IP address: %s", s.c_str());
        reconnectAttempts = 0; // Reset attempts on successful connection
        connected = true;
    }
    break;

    default:
        break;
    }
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO("WiFi connected, IP address: %s ", WiFi.localIP().toString().c_str());
    connected = true;
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_ERROR("Disconnected from WiFi access point");
    LOG_ERROR("WiFi lost connection. Reason: %d", info.wifi_sta_disconnected.reason);
    // Handle the disconnection based on the reason code
    switch (info.wifi_sta_disconnected.reason)
    {
    case WIFI_REASON_UNSPECIFIED:
        LOG_ERROR("Unspecified reason.");
        break;
    case WIFI_REASON_AUTH_EXPIRE:
        LOG_ERROR("Authentication expired.");
        break;
    case WIFI_REASON_AUTH_LEAVE:
        LOG_ERROR("Station left the authentication.");
        break;
    case WIFI_REASON_ASSOC_EXPIRE:
        LOG_ERROR("Association expired.");
        break;
    case WIFI_REASON_ASSOC_LEAVE:
        LOG_ERROR("ESP32 left the association with the AP. Attempting to reconnect...");
        break; 
    case WIFI_REASON_NO_AP_FOUND:
        LOG_ERROR("No Access Point found.");
        break;
    case WIFI_REASON_AUTH_FAIL:
        LOG_ERROR("Authentication failure.");
        break;
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        LOG_ERROR("Handshake timeout.");
        break;
    // Add more cases as needed
    default:
        LOG_ERROR("Other reason.");
        break;
    }

    LOG_ERROR("Trying to Reconnect");
    connected = false;
    // WiFi.begin(ssid, password);
}

static long lastReconnectAttempt = 0;

void handleWiFiReconnect()
{
    unsigned long currentMillis = millis();
 
    if (reconnectAttempts < WIFI_NUMBER_OF_TRIES)
    {
        int delayTime = min((int)5000 * reconnectAttempts, 60000); // Exponential backoff with a maximum delay of 60 seconds
        if (currentMillis - lastReconnectAttempt >= delayTime)
        {
            reconnectAttempts++;
            LOG_ERROR("WLAN::handleWiFiReconnect - Reconnecting in %d seconds", delayTime / 1000);

            // delay(delayvTime);

            LOG_ERROR("WLAN::handleWiFiReconnect Reconnecting to WiFi...");
            WiFi.begin(ssid, password);
            lastReconnectAttempt = currentMillis;
            LOG_ERROR("WLAN::handleWiFiReconnect - result of reconnecting.. ");
        }
        else
        {
            LOG_DEBUG("WLAN::handleWiFiReconnect , reconnect attemps: %d  ", reconnectAttempts);
        }
    }

    else
    {
        LOG_INFO("Max reconnect attempts reached. Can not reconnect to WiFi. (restart)");
        reconnectAttempts = 0;
        // Optionally, reset the ESP32 to attempt a fresh connection
        ESP.restart();
    }
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

    WiFi.persistent(false); // Schont den Flash-Speicher bei häufigen Reconnects
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(NET_HOSTNAME);
    WiFi.disconnect();
    readMacAddress();
    wifi_scan_network();
    ssid = setup.ssid;
    password = setup.passwd;
    connected = false;

    // Event-Handler registrieren
    WiFi.onEvent(WiFiStationConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(WiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.begin(setup.ssid, setup.passwd);;
    WiFi.begin(ssid, password);

    // LOG_DEBUG("WIFI: %s, Passwd: %s", setup.ssid, setup.passwd);
    //LOG_DEBUG("wlan::wifi_init - Connecting to WiFi ..");
    tft_printInfo("Connecting to WiFi :: delay: 10 secs!!");
    //delay(10000);
    wiFiStatus = WiFi.status();
    DBGf("wlan::wifi_init() connect - status: %d", wiFiStatus);

    if (WiFi.status() != WL_CONNECTED && reconnectAttempts >= WIFI_NUMBER_OF_TRIES)
    {
        LOG_INFO("Trying to reconnect...");
        reconnectAttempts = 0;
        handleWiFiReconnect();
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
    handleWiFiReconnect();
    if (connected)
        return true;
    return false;

/*     int numberOfTries = WIFI_NUMBER_OF_TRIES;
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
 */}
