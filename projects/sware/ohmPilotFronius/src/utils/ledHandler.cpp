#include <Arduino.h>
#include "ledHandler.h"

#include "pin_config.h"

#define MODBUS_ERROR 0
#define CARD_READ_ERROR 1
#define TEMPERATUR_ERROR 2

static unsigned bitfield = 0;

void ledHandler_init()
{
    pinMode(LED_ERROR1, OUTPUT);
    pinMode(LED_ERROR2, OUTPUT);
    digitalWrite(LED_ERROR1, LOW);
    digitalWrite(LED_ERROR2, LOW);
}

void ledHandler_showModbusError(bool enable)
{

    if (enable)
    {
        digitalWrite(LED_ERROR1, HIGH);
        bitfield |= (1 << MODBUS_ERROR); // Set the third bit to 1
    }
    else
    {

        bitfield &= ~(1 << MODBUS_ERROR); // Clear the third bit to 0
        digitalWrite(LED_ERROR1, LOW);
    }
}

void ledHandler_showCardReaderError(bool enable)
{
    if (enable)
    {
        digitalWrite(LED_ERROR2, HIGH);
        bitfield |= (1 << CARD_READ_ERROR); // Set the third bit to 1
    }

    else
    {
        bitfield &= ~(1 << CARD_READ_ERROR); // Clear the third bit to 0
        digitalWrite(LED_ERROR2, LOW);
    }
}
void ledHandler_showTemperaturError(bool enable)
{

    if (enable)
    {
        bitfield |= (1 << TEMPERATUR_ERROR); // Set the third bit to 1
        digitalWrite(LED_ERROR2, HIGH);
        digitalWrite(LED_ERROR1, HIGH);
    }
    else
    {
        digitalWrite(LED_ERROR2, LOW);
        digitalWrite(LED_ERROR1, LOW);
        bitfield &= ~(1 << TEMPERATUR_ERROR); // Clear the third bit to 0
    }
}

void ledHandler_help_blink(byte bitErrorCode, uint8_t led)
{
    int status = 0;
    if ((bitfield & (1 << bitErrorCode)) != 0)
    {
        status = digitalRead(led);
        if (status == HIGH)
            digitalWrite(led, LOW);
        else
            digitalWrite(led, LOW);
    }
}

void ledHandler_blink()
{
    if (bitfield > 0)
    {
        ledHandler_help_blink(MODBUS_ERROR, LED_ERROR1);
        ledHandler_help_blink(CARD_READ_ERROR, LED_ERROR2);
        if ((bitfield & (1 << TEMPERATUR_ERROR)) != 0)
        {
            ledHandler_help_blink(TEMPERATUR_ERROR, LED_ERROR1);
            ledHandler_help_blink(TEMPERATUR_ERROR, LED_ERROR2);
        }
    }
}