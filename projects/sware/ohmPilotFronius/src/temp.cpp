
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
/*
Found device 0 with address:  28 9A 2C 570 4 E1 3C D5   -- lila pickerl
Found device 1 with address:  28 BE 5C 570 4 E1 3C D2

*/
DeviceAddress sensor1 = {0x28, 0x9A, 0x2C, 0x57, 0x4, 0xE1, 0x3C, 0xD5};
DeviceAddress sensor2 = {0x28, 0xBE, 0x5C, 0x57, 0x4, 0xE1, 0x3C, 0xD2};
// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        Serial.print(" ");
        Serial.print(deviceAddress[i], HEX);
    }
}
void temp_init()
{
    int numberOfDevices = 0;
    DeviceAddress tempDeviceAddress;
    Serial.println("Init Temp Sensor...");
    sensors.setResolution(11);
    sensors.begin();
    // Grab a count of devices on the wire
    numberOfDevices = sensors.getDeviceCount();

    // locate devices on the bus
    Serial.print("Locating devices...");
    Serial.print("Found ");
    Serial.print(numberOfDevices, DEC);
    Serial.println(" devices.");

    // Loop through each device, print out address
    for (int i = 0; i < numberOfDevices; i++)
    {
        // Search the wire for address
        if (sensors.getAddress(tempDeviceAddress, i))
        {
            Serial.print("Found device ");
            Serial.print(i, DEC);
            Serial.print(" with address: ");
            printAddress(tempDeviceAddress);
            Serial.println();
        }
        else
        {
            Serial.print("Found ghost device at ");
            Serial.print(i, DEC);
            Serial.print(" but could not detect address. Check power and cabling");
        }
    }
}

bool temp_getTemperature(TEMPERATURE &container)
{
    sensors.requestTemperatures(); // Send the command to get temperatures
    // Serial.println("DONE");

    // Serial.print("Sensor 1(*C): ");
    delay(400);
    container.sensor1 = sensors.getTempC(sensor1);
    container.sensor2 = sensors.getTempC(sensor2);
    /* float tempC = sensors.getTempCByIndex(0);
    float tempC1 = sensors.getTempCByIndex(1);
    Serial.print("Sensor 1: ");
    Serial.print(tempC);
    Serial.print(", Sensor 2: ");
    Serial.println(tempC1); */
    /* Serial.print(temperatureF);
    Serial.print(" Sensor 1(*F): ");
    Serial.println(sensors.getTempF(sensor1)); */
    return true;
}