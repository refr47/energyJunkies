#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>




bool wifi_init();

/*   DEFInES
 */
#define INTERNAL_BUTTON_1_GPIO 0
#define INTERNAL_BUTTON_2_GPIO 35

#define WIFI_TRY_DELAY 500
#define WIFI_NUMBER_OF_TRIES 20
/* ***********************************
  GLOBAL VARS
*/

TFT_eSPI tft = TFT_eSPI();
#define SSID "WLAN-HTLW"
#define PASSWD "HTL-Wels"

void setup()
{
  tft.init();
  // tft.setRotation(1);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  Serial.begin(115200);
  delay(10);

  /*enable internal buttons*/
  pinMode(INTERNAL_BUTTON_1_GPIO, INPUT_PULLUP);
  pinMode(INTERNAL_BUTTON_2_GPIO, INPUT_PULLUP);
  tft.setCursor(0, 0, 4);
  if (!wifi_init())
    tft.println("Cannot connect");
  else
    tft.println("Connected");
  tft.println(WiFi.localIP());
}

void loop()
{
  // put your main code here, to run repeatedly:

  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.setCursor(0, 80, 4);

  tft.println("Hello");

  delay(200);
}

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