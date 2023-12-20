#include <Arduino.h>
#include "ledHandler.h"

#include "pin_config.h"

#define MODBUS_ERROR 0
#define CARD_READ_ERROR 1
#define TEMPERATUR_ERROR 2

static unsigned bitfield = 0;

static bool errorHappened = false;

static bool errorLedModbus = false;
static bool errorLedCardReader = false;
static bool errorLedTemperature = false;

void ledHandler_init()
{
    pinMode(LED_ERROR, OUTPUT);
    pinMode(LED_WORKING, OUTPUT);
    digitalWrite(LED_ERROR, LOW);
    digitalWrite(LED_WORKING, LOW);
}

void ledHandler_showModbusError(bool enable)
{

    if (enable)
    {
        digitalWrite(LED_ERROR, HIGH);
        bitfield |= (1 << MODBUS_ERROR); // Set the third bit to 1
    }
    else
    {

        bitfield &= ~(1 << MODBUS_ERROR); // Clear the third bit to 0
        digitalWrite(LED_ERROR, LOW);
    }
}

void ledHandler_showCardReaderError(bool enable)
{
    errorLedCardReader = errorHappened = enable;
    if (enable)
        digitalWrite(LED_WORKING, HIGH);
    else
        digitalWrite(LED_WORKING, LOW);
}
void ledHandler_showTemperaturError(bool enable)
{
    errorLedTemperature = errorHappened = enable;
    errorLedModbus = errorHappened = enable;
    if (enable)
    {
        digitalWrite(LED_WORKING, HIGH);
        digitalWrite(LED_ERROR, HIGH);
    }
    else
    {
        digitalWrite(LED_WORKING, LOW);
        digitalWrite(LED_ERROR, LOW);
    }
}

void ledHandler_blink()
{
    if (errorHappened)
    {
        errorLedModbus = !errorLedModbus;
        digitalWrite(LED_ERROR, errorLedModbus == true ? LOW : HIGH);

        errorLedCardReader = !errorLedCardReader;
        digitalWrite(LED_WORKING, errorLedCardReader == true ? LOW : HIGH);
    }
}