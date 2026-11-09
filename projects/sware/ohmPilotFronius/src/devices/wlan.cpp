#define _WIFI_CPP

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <tft.h>
#include "wlan.h"
#include "defines.h"

static const char *Get_WiFiStatus(int status);

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO("TAG_WLAN", "wlan::WiFiStationConnected - Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO("TAG_WLAN", "WiFi connected, IP address: %s ", WiFi.localIP().toString().c_str());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_ERROR("TAG_WLAN", "Disconnected from WiFi access point");
    LOG_ERROR("TAG_WLAN", "WiFi lost connection. Reason: %d", info.wifi_sta_disconnected.reason);
    // Handle the disconnection based on the reason code
    switch (info.wifi_sta_disconnected.reason)
    {
    case WIFI_REASON_UNSPECIFIED:
        LOG_ERROR("TAG_WLAN", "Unspecified reason.");
        break;
    case WIFI_REASON_AUTH_EXPIRE:
        LOG_ERROR("TAG_WLAN", "Authentication expired.");
        break;
    case WIFI_REASON_AUTH_LEAVE:
        LOG_ERROR("TAG_WLAN", "Station left the authentication.");
        break;
    case WIFI_REASON_ASSOC_EXPIRE:
        LOG_ERROR("TAG_WLAN", "Association expired.");
        break;
    case WIFI_REASON_ASSOC_LEAVE:
        LOG_ERROR("TAG_WLAN", "ESP32 left the association with the AP. Attempting to reconnect...");
        break;
    case WIFI_REASON_NO_AP_FOUND:
        LOG_ERROR("TAG_WLAN", "No Access Point found.");
        break;
    case WIFI_REASON_AUTH_FAIL:
        LOG_ERROR("TAG_WLAN", "Authentication failure.");
        break;
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        LOG_ERROR("TAG_WLAN", "Handshake timeout.");
        break;
    // Add more cases as needed
    default:
        LOG_ERROR("TAG_WLAN", "Other reason.");
        break;
    }

    LOG_ERROR("TAG_WLAN", "Trying to Reconnect");
}

static const char *Get_WiFiStatus(int status)
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
    default:
        return "UNKNOWN";
    }
}

bool wifi_init(Setup &setup)
{
    LOG_DEBUG("TAG_WLAN", "wlan:: wifi_init()");

    WiFi.persistent(false); // Schont den Flash-Speicher bei häufigen Reconnects
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(NET_HOSTNAME);
    // taskWiFi owns retries; a second automatic reconnect can reset an
    // in-progress association and repeatedly produce "Set status to INIT".
    WiFi.setAutoReconnect(false);
    // A scan here disconnects the station and is only diagnostic. WiFi.begin()
    // selects the configured SSID itself, regardless of other visible networks.

    // Event-Handler registrieren
    WiFi.onEvent(WiFiStationConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(WiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    LOG_INFO(TAG_WLAN, "Connecting to configured SSID: %s", setup.ssid);
    WiFi.begin(setup.ssid, setup.passwd);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 30)
    { // 30 * 500ms = 15s
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
        Serial.print(".");
    }

    tft_printInfo("Connecting to WiFi");
    const int wiFiStatus = WiFi.status();
    DBGf("wlan::wifi_init() connect - status: %d, %s", wiFiStatus, Get_WiFiStatus(wiFiStatus));

    if (wiFiStatus == WL_CONNECTED)
    {
        LOG_INFO("TAG_WLAN", "wlan::WIFI connected ");

        String s = WiFi.localIP().toString();
        Serial.print(WiFi.localIP());
        DBGf("TAG_WLAN", "wlan::WIFI connected with IP: %s ", s.c_str());

        strcpy(setup.currentIP, WiFi.localIP().toString().c_str());
        return true;
    }
    else
    {
        LOG_ERROR(TAG_WLAN, "Cannot connect to network ssid: %s", setup.ssid);
        tft_printInfo("", true);
        tft_printInfo("WiFi timeout - starting setup AP");
        return false;
    }

}

void wifi_scan_network(const char *configuredSsid)
{
    // Called only after a failed connection attempt. Do not change mode or
    // disconnect: either action would reset STA association or the setup AP.
    LOG_INFO(TAG_WLAN, "Scanning visible WLANs (configured: %s)", configuredSsid ? configuredSsid : "-");
    int16_t n = WiFi.scanNetworks();

    if (n < 0)
    {
        LOG_ERROR(TAG_WLAN, "WiFi scan failed: %d", n);
    }
    else if (n == 0)
    {
        LOG_INFO(TAG_WLAN, "No WLAN networks found");
    }
    else
    {
        LOG_INFO(TAG_WLAN, " ======   WLAN Networks ============");
        // tft_getRoot().setTextDatum(TL_DATUM);
        // tft_getRoot().setCursor(0, 0, 4);
        const char *cp = "";
        LOG_INFO(TAG_WLAN, "Found %d net", n);
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
            const String ssid = WiFi.SSID(i);
            const bool configured = configuredSsid && ssid == configuredSsid;
            LOG_INFO(TAG_WLAN, "[%d] %s (%ld dBm, %s)%s", i + 1, ssid.c_str(),
                static_cast<long>(WiFi.RSSI(i)), cp, configured ? " <- configured" : "");
        }
    }
    WiFi.scanDelete();
}

void wifi_getLocalIP(char **pBuffer16)
{
    IPAddress localIP = WiFi.localIP();
    localIP.toString().toCharArray(*pBuffer16, INET_ADDRSTRLEN);
}

bool wifi_isStillConnected()
{
    const int wiFiStatus = WiFi.status();
    if (wiFiStatus == WL_CONNECTED)
    {
        return true;
    }
    else
    {
        switch (wiFiStatus)
        {
            {
            case WL_IDLE_STATUS:
                LOG_ERROR(TAG_WLAN, "Running idle ");
                break;
            case WL_NO_SSID_AVAIL:
                LOG_ERROR(TAG_WLAN, "SSID not found");
                break;
            case WL_SCAN_COMPLETED:
                LOG_ERROR(TAG_WLAN, "SCAN done successfully");
                break;
            case WL_CONNECT_FAILED:
                LOG_ERROR(TAG_WLAN, "Connection did not succeed - wrong password?");
                break;
            case WL_DISCONNECTED:
                LOG_ERROR(TAG_WLAN, "Disconnected");
                break;
            default:
                LOG_ERROR(TAG_WLAN, "Unknown error: %d", wiFiStatus);
                break;
            }
        }
        // taskWiFi is the only code that starts another connection attempt.
    }

    return false;
}
