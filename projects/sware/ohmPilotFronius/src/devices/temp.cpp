
#include <OneWire.h>
#include <DallasTemperature.h>

#include "pin_config.h"
#include "temp.h"
#include "debugConsole.h"

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
    LOG_DEBUG("temperature::printAddress");
    char buff[100];
    sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", deviceAddress[0], deviceAddress[1], deviceAddress[2], deviceAddress[3], deviceAddress[4], deviceAddress[5], deviceAddress[6], deviceAddress[7]);
    /*  for (uint8_t i = 0; i < 8; i++)
     {
         buf[i]=deviceAddress[i];
     } */
    LOG_DEBUG(" %s", buff);
}
bool temp_init()
{
    int numberOfDevices = 0;
    DeviceAddress tempDeviceAddress;
    LOG_INFO("temperature::Init Temp Sensor...");
    bool result;
    sensors.setResolution(11);
    sensors.begin();
    // Grab a count of devices on the wire
    numberOfDevices = sensors.getDeviceCount();
    if (numberOfDevices == 0)
    {
        LOG_ERROR("temp_init() - keine Temperatursensorik gefunden.");
        return false;
    }
    // locate devices on the bus
    LOG_INFO("temperature:Locating devices...Found :%d devices", numberOfDevices);

    // Loop through each device, print out address
    for (int i = 0; i < numberOfDevices; i++)
    {
        // Search the wire for address
        if (sensors.getAddress(tempDeviceAddress, i))
        {
            LOG_DEBUG("Found device %d with address:", i);

            printAddress(tempDeviceAddress);
        }
        else
        {
            LOG_ERROR("temperature::Init Temp Sensor - but could not detect address. Found ghost device at  %d", i);

            LOG_DEBUG("temp_init() - keine Temperatursensorik gefunden.");
        }
    }

    return true;
}

bool temp_getTemperature(TEMPERATURE &container)
{
    sensors.requestTemperatures(); // Send the command to get temperatures
    // DBGln("DONE");

    // DBG("Sensor 1(*C): ");
    delay(1000);
    container.sensor1 = sensors.getTempC(sensor1);
    container.sensor2 = sensors.getTempC(sensor2);
    if (container.sensor1 < 0)
    {
        ESP_LOGE(TAG, "temp_getTemperature - Temperatur Sensor 1 (%.2f) kann nicht negativ sein.", container.sensor1);
    }
    if (container.sensor2 < 0)
    {
        ESP_LOGE(TAG, "temp_getTemperature - Temperatur Sensor 2 (%.2f) kann nicht negativ sein.", container.sensor1);
    }
    if (container.sensor1 < 0 && container.sensor2 < 0)
        return false;
    if (container.sensor1 > 0)
    {
        container.sensor2 = container.sensor1;
    }
    else

    {
        container.sensor1 = container.sensor2;
    }

    /* float tempC = sensors.getTempCByIndex(0);
    float tempC1 = sensors.getTempCByIndex(1);
    DBG("Sensor 1: ");
    DBG(tempC);
    DBG(", Sensor 2: ");
    DBGln(tempC1); */
    /* DBG(temperatureF);
    DBG(" Sensor 1(*F): ");
    DBGln(sensors.getTempF(sensor1)); */
    return true;
}

int temp_getNumberOfDevices()
{
    return 2;
}