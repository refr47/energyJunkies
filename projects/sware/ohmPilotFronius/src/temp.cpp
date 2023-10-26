
#include <OneWire.h>
#include <DallasTemperature.h>
#include "pin_config.h"
#include "temp.h"

// GPIO where the DS18B20 is connected to
// SENSOR 1: 28 9A 2C 57 4 E1 3C D5

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_TEMP_GPIO);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1 = {0x28, 0x9A, 0x2C, 0x57, 0x4, 0xE1, 0x3C, 0xD5};

void temp_init()
{
    byte i;
    byte addr[8];
    Serial.println("Init Temp Sensor...");
    sensors.begin();
    sensors.setResolution(9);
    pinMode(ONE_WIRE_TEMP_GPIO, INPUT_PULLUP);
    digitalWrite(ONE_WIRE_TEMP_GPIO, LOW);
    delay(1000);
    digitalWrite(ONE_WIRE_TEMP_GPIO, HIGH);
    delay(1000);

    if (!oneWire.search(addr))
    {
        Serial.println(" No more addresses.");
        Serial.println();
        oneWire.reset_search();
        delay(250);
        return;
    }
    Serial.print(" ROM =");
    for (i = 0; i < 8; i++)
    {
        Serial.write(' ');
        Serial.print(addr[i], HEX);
    }
}
float getTempSensor1()
{
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("DONE");

    Serial.print("Sensor 1(*C): ");
    float temperatureF = sensors.getTempC(sensor1);
    Serial.print(temperatureF);
    Serial.print(" Sensor 1(*F): ");
    Serial.println(sensors.getTempF(sensor1));
    return temperatureF;
}