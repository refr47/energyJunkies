#include <Arduino.h>
#include "ledHandler.h"

#include "pin_config.h"

static bool errorHappened = false;
static bool errorLed = false;

void ledHandler_init()
{
    pinMode(LED_ERROR, OUTPUT);
    pinMode(LED_WORKING, OUTPUT);
    digitalWrite(LED_ERROR, LOW);
    digitalWrite(LED_WORKING, LOW);
}

void ledHandler_showError(bool enable)
{
    errorLed = errorHappened = enable;
    if (enable)
        digitalWrite(LED_ERROR, HIGH);
    else
        digitalWrite(LED_ERROR, LOW);
}

void ledHandler_blink()
{
    if (errorHappened)
    {
        errorLed = !errorLed;
        digitalWrite(LED_ERROR, errorLed == true ? LOW : HIGH);
    }
}