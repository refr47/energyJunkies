/*
  Modbus-Arduino Example - Master Modbus IP Client (ESP8266/ESP32)
  Read Holding Register from Server device

  (c)2018 Alexander Emelianov (a.m.emelianov@gmail.com)
  https://github.com/emelianov/modbus-esp8266

  Negativ zu positiv nach Vanessa
  Phasenanschnitt hinzugefügt



  Libraries:
    PID_v1
    Modbus-esp8266


*/

#include "output.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <ModbusIP_ESP8266.h>

#include <esp_now.h>
#include <SPI.h>

#include <TFT_eSPI.h>  // Hardware-specific library

//TODO: fix pin numbers
// Pin numbers for outputs
#define DIGOUT1_PIN   10
#define DIGOUT2_PIN   11
#define ANOUT_PIN     12

#define DEVICE_NAME_LEN 24


// TFT_eSPI tft = TFT_eSPI();  // Invoke custom library

OutputManager out = OutputManager(DIGOUT1_PIN, DIGOUT2_PIN, ANOUT_PIN);
int testzahl = -12;



typedef struct {
  int deviceId;                // modbus device id: inverter: 1, smart meter: 200
  int baseAddr;                // register number (already reduced by 1)
  int count;                   // number of registers to read
  char text[DEVICE_NAME_LEN];  // device name
} modbus_read_t;

// number of register blocks to read
#define REG_BLOCK_COUNT 2
#define INVERTER_ID 1
#define SMART_METER_ID 200
int powerganzzahl = 0;
float power = 0;

// inverter is device id 1, storage registers start at 400345, read 24 registers
// smart meter is device 200, meter registers start at 40087, read 5 registers
modbus_read_t regsToRead[REG_BLOCK_COUNT] = { { INVERTER_ID, 40345, 24, "Inverter" }, { SMART_METER_ID, 40087, 5, "Smart Meter" } };
const int regsCount = 24;  // höchste Registeranzahl aus obiger Lesezugriffstabelle


IPAddress remote(192, 168, 1, 5);  // Address of Modbus Slave device

ModbusIP mb;  //ModbusIP object

char text[256];
char buffer[32];  //Buffer zum umrechnen einstellig auf zweistellig
int maxpower = 5950;
int minpower = 5900;
int phasenanschnitt = 0;
int pwmpin = 12;
int leistungsstuffe1 = 0;
int leistungsstuffe2 = 0;

void setup() {
  pinMode(pwmpin, OUTPUT);
  // tft.init();
  // tft.setRotation(1);

  // tft.fillScreen(TFT_BLACK);


  // tft.setCursor(0, 0, 4);
  // tft.setTextColor(TFT_BLUE, TFT_BLACK);
  // tft.println("Moarli Test");

  // delay(500);

  // tft.setTextSize(1);
  // //tft.fillScreen(TFT_BLACK);
  // tft.setCursor(0, 30, 4);
  // tft.setTextColor(TFT_RED, TFT_BLACK);
  // tft.println("warten auf WLAN");
  // delay(10);

  Serial.begin(115200);
  Serial.println("=== Startup ===");
  // WiFi.begin("Moar z'Viert", "Dusty3101");

  // Serial.print("Connecting to WIFI network ");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }

  // Serial.println(" succeeded");
  // Serial.print("IP address: ");
  // Serial.println(WiFi.localIP());

  // mb.client();
}

uint16_t res = 0;
int16_t resArr[REG_BLOCK_COUNT][regsCount];



void loop() {

  static int cnt = 0;
  cnt++;

  if(cnt > 20){
    // powerganzzahl = -10;
  } else if (cnt > 10) {
    powerganzzahl = 400;
  } else {
    powerganzzahl = 200;
  }
  out.task(powerganzzahl);




delay(1000);





  // static int readIndex = 0;

  // uint16_t transId = 0;
  // if (mb.isConnected(remote)) {  // Check if connection to Modbus Slave is established
  //   //Serial.println("Modbus/TCP connected");
  //   //mb.readHreg(remote, REG, &res);  // Initiate Read Coil from Modbus Slave
  //   transId = mb.readHreg(remote, regsToRead[readIndex].baseAddr, (uint16_t *)resArr[readIndex], regsToRead[readIndex].count, NULL, regsToRead[readIndex].deviceId);  // Initiate Read Holding Register from Modbus Slave
  //   if (transId == 0) {
  //     Serial.println("Modbus/TCP register read failed");
  //     //    } else {
  //     //      Serial.println("Modbus/TCP register read succeeded");
  //   }
  // } else {
  //   bool success = mb.connect(remote);  // Try to connect if no connection
  //   Serial.println(success ? "Successfully connected to Modbus server" : "Failed to connect to Modbus server");
  // }
  // mb.task();  // Common local Modbus task
  // if (transId != 0) {
  //   delay(300);  // Pulling interval
  //   Serial.println();
  //   Serial.println(regsToRead[readIndex].text);
  //   for (int i = 0; i < regsToRead[readIndex].count; i++) {
  //     Serial.print(regsToRead[readIndex].baseAddr + i);
  //     Serial.print('\t');
  //     if (regsToRead[readIndex].deviceId == SMART_METER_ID && i == 0) {
  //       power = resArr[readIndex][i] * pow(10, resArr[readIndex][4]);
  //       //############# Anzeige ##########################
  //       int powerganzzahl = (int)(power + .5);  //Umwandeln in ganze Zahl
  //       powerganzzahl = powerganzzahl * -1;     //Umwandeln Negativ ins positive

  //       tft.fillScreen(TFT_BLACK);


  //       if (powerganzzahl <= 0) {
  //         tft.setTextSize(1);
  //         tft.setTextColor(TFT_RED, TFT_BLACK);
  //         tft.setCursor(30, 0, 4);
  //         tft.println("Bezug KWG");
  //         phasenanschnitt = 0;
  //       }

  //       if (powerganzzahl >= 0) {
  //         tft.setTextSize(1);
  //         tft.setTextColor(TFT_GREEN, TFT_BLACK);
  //         tft.setCursor(0, 0, 4);
  //         tft.println("OEMAG");

  //         if (powerganzzahl >= maxpower) {

  //           phasenanschnitt++;
  //           if (phasenanschnitt >= 255) {
  //             phasenanschnitt = 255;
  //           }
  //           analogWrite(pwmpin, phasenanschnitt);
  //         }

  //         if (powerganzzahl <= minpower) {
  //           phasenanschnitt--;
  //           if (phasenanschnitt <= 0) {
  //             phasenanschnitt = 0;
  //           }
  //           analogWrite(pwmpin, phasenanschnitt);
  //         }



  //         tft.setTextSize(1);
  //         tft.setTextColor(TFT_GREEN, TFT_BLACK);
  //         tft.setCursor(0, 83, 7);
  //         tft.println(phasenanschnitt);

  //         if (phasenanschnitt >= 255) {
  //           leistungsstuffe1 = 1;
  //         }
  //       }



  //       //####### Löschen der stehengebliebenen Zahlen ##################
  //       tft.setTextSize(1);
  //       tft.setTextColor(TFT_BLUE, TFT_BLACK);
  //       tft.setCursor(0, 30, 7);
  //       tft.println("            ");



  //       tft.setTextSize(1);
  //       // tft.fillScreen(TFT_BLACK);  //last change
  //       tft.setTextColor(TFT_BLUE, TFT_BLACK);
  //       tft.setCursor(10, 30, 7);
  //       tft.println(powerganzzahl);


  //       tft.setTextSize(1);
  //       tft.setTextColor(TFT_GREEN, TFT_BLACK);
  //       tft.setCursor(0, 83, 7);
  //       tft.println(phasenanschnitt);




  //       // scale real power by it's scale factor
  //       Serial.println(resArr[readIndex][i] * pow(10, resArr[readIndex][4]));
  //     } else {
  //       Serial.println(resArr[readIndex][i]);
  //     }
  //   }
  //   readIndex = (readIndex + 1) % REG_BLOCK_COUNT;
  // }
}
