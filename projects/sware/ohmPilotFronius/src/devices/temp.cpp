
#include <OneWire.h>
#include <DallasTemperature.h>

#include "pin_config.h"
#include "temp.h"
#include "debugConsole.h"

// GPIO where the DS18B20 is connected to
// SENSOR 1: 28 9A 2C 57 4 E1 3C D5

// static int tempHistory[TEMP_FILTER_SIZE];
// static int filterIdx = 0;

// Setup a oneWire instance to communicate with any OneWire devices
static OneWire oneWire(ONE_WIRE_TEMP_GPIO);

// Pass our oneWire reference to Dallas Temperature sensor
static DallasTemperature sensors(&oneWire);
static int numberOfDevices = 0;
static DeviceAddress tempDeviceAddress;
/*c
Found device 0 with address:  28 9A 2C 570 4 E1 3C D5   -- lila pickerl
Found device 1 with address:  28 BE 5C 570 4 E1 3C D2

*/
/* DeviceAddress sensor1 = {0x28, 0x9A, 0x2C, 0x57, 0x4, 0xE1, 0x3C, 0xD5};
DeviceAddress sensor2 = {0x28, 0xBE, 0x5C, 0x57, 0x4, 0xE1, 0x3C, 0xD2}; */
// function to print a device address

struct TempFilter
{
    float values[TEMP_FILTER_SIZE]; // Die letzten 5 Messwerte
    int index = 0;
    bool filled = false;

    float add(float newValue)
    {
        values[index] = newValue;
        index = (index + 1) % TEMP_FILTER_SIZE;
        if (index == 0)
            filled = true;

        int count = filled ? TEMP_FILTER_SIZE : index;
        int sum = 0;
        for (int i = 0; i < count; i++)
            sum += values[i];
        return sum / count;
    }
};

static TempFilter filter1, filter2;

static void hardware_reset();

void printAddress(DeviceAddress deviceAddress)
{
    LOG_DEBUG(TAG_TEMP, "temperature::printAddress");
    char buff[100];
    sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", deviceAddress[0], deviceAddress[1], deviceAddress[2], deviceAddress[3], deviceAddress[4], deviceAddress[5], deviceAddress[6], deviceAddress[7]);
    /*  for (uint8_t i = 0; i < 8; i++)
     {
         buf[i]=deviceAddress[i];
     } */
    LOG_DEBUG(TAG_TEMP, " %s", buff);
}

void search()
{
    LOG_DEBUG(TAG_TEMP, "temperature::search");
    byte addr[8];
    while (oneWire.search(addr))
    {
        Serial.print("Found device with address: ");
        for (int i = 0; i < 8; i++)
        {
            if (addr[i] < 16)
                Serial.print("0");
            Serial.print(addr[i], HEX);
        }
        Serial.println();
    }
    oneWire.reset_search();
}

bool temp_init()
{
    /* Serial.print("\n-----------------------------: \n\n");
    delay(3000);
    search();
    delay(3000); */

    LOG_INFO(TAG_TEMP, "temperature::Init Temp Sensor...");

    sensors.setResolution(11);
    sensors.begin();
    // Grab a count of devices on the wire
    numberOfDevices = sensors.getDeviceCount();

    if (numberOfDevices == 0)
    {
        LOG_ERROR(TAG_TEMP, "temp_init() - keine Temperatursensorik gefunden.");
        printf("temp-init:: keine Sensorik gefunden\n");
        return false;
    }

    // locate devices on the bus
    LOG_INFO(TAG_TEMP, "temperature:Locating devices...Found :%d devices", numberOfDevices);

    // Loop through each device, print out address
    for (int i = 0; i < numberOfDevices; i++)
    {
        // Search the wire for address
        if (sensors.getAddress(tempDeviceAddress, i))
        {
            LOG_DEBUG(TAG_TEMP, "Found device %d with address:", i);

            printAddress(tempDeviceAddress);
        }
        else
        {
            LOG_ERROR(TAG_TEMP, "temperature::Init Temp Sensor - but could not detect address. Found ghost device at  %d", i);

            LOG_DEBUG(TAG_TEMP, "temp_init() - keine Temperatursensorik gefunden.");
        }
    }

    return true;
}
static int errorCounter = 0; // Zählt aufeinanderfolgende Fehler

bool temp_getTemperature(TEMPERATURE &container)
{
    if (numberOfDevices == 0)
    {
        if (!temp_init())
        {
            LOG_INFO(TAG_TEMP, "Keine Sensoren gefunden!");
            return false;
        }
    }

    // Command an alle Sensoren: Messung starten
    sensors.requestTemperatures();

    // Warten, bis die Sensoren fertig sind (750ms bei 12-bit)
    // vTaskDelay ist besser als delay(), da es die CPU für andere Tasks freigibt
    vTaskDelay(pdMS_TO_TICKS(800));

    bool success1 = false;
    bool success2 = false;

    for (int i = 0; i < numberOfDevices; i++)
    {
        float rawTemp = sensors.getTempCByIndex(i);

        // Validierung der DS18B20 Standard-Fehlerwerte
        // -127.0: Sensor nicht erreichbar / Kabelbruch
        // 85.0:   Sensor hat Saft, aber noch keine Messung abgeschlossen
        if (rawTemp == DEVICE_DISCONNECTED_C || rawTemp == 85.0 || rawTemp < -10.0)
        {
            LOG_INFO(TAG_TEMP, "Sensor %d liefert ungültigen Wert: %.2f", i, rawTemp);
        }
        else
        {

            if (i == 0)
            {
                container.sensor1 = filter1.add(rawTemp);
                success1 = true;
            }
            else if (i == 1)
            {
                container.sensor2 = filter2.add(rawTemp);
                success2 = true;
            }
        }
    }

    if (! (success1 && success2)) 
    {
        LOG_ERROR(TAG_TEMP, "Alle Messungen fehlgeschlagen. Reinit...");
        errorCounter++;
        if(errorCounter < 5)
        {
            // Die Werte in container.sensor1/2 bleiben einfach die alten vom letzten Mal
            return true;    
            
        }
        else
        {
            // Erst nach 5 Fehlern in Folge melden wir einen harten Fehler
            LOG_ERROR(TAG_TEMP, "5 Fehler in Folge! Resetting Bus...");
            hardware_reset();
            temp_init();
            errorCounter = 0; // Reset counter um Endlosschleife zu vermeiden
            
        }
    } else {
        errorCounter = 0; // Reset counter bei erfolgreicher Messung
    }
    return success1 && success2;
}

int temp_getNumberOfDevices()
{
    return numberOfDevices;
}

static void hardware_reset()
{
    LOG_DEBUG(TAG_TEMP, "temperature::hardware_reset - Resetting OneWire Bus");
    pinMode(ONE_WIRE_TEMP_GPIO, OUTPUT);
    digitalWrite(ONE_WIRE_TEMP_GPIO, LOW); // Kurzschluss gegen GND
    delayMicroseconds(480);                // Mind. 480µs (Reset-Zeit)
    pinMode(ONE_WIRE_TEMP_GPIO, INPUT);    // Wieder auf Eingang schalten
    // Reset the OneWire bus
    oneWire.reset();
    oneWire.reset_search(); // Suchstatus zurücksetzen
    // Reinitialize the sensors
}
