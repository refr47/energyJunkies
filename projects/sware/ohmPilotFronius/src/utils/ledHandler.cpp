#include <Arduino.h>
#include "ledHandler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MODBUS_ERROR 0
#define CARD_READ_ERROR 1
#define TEMPERATUR_ERROR 2
#define NETWORK_ERROR 3

static uint8_t globalBitfield = 0;
static unsigned globalBlink = 0;

void ledHandler_init()
{
    pinMode(LED_ERROR1, OUTPUT);
    pinMode(LED_ERROR2, OUTPUT);
    digitalWrite(LED_ERROR1, LOW);
    digitalWrite(LED_ERROR2, LOW);
    vTaskDelay(pdMS_TO_TICKS(200));
    for (int jj = 0; jj < 10; jj++)
    {
        digitalWrite(LED_ERROR1, HIGH);
        digitalWrite(LED_ERROR2, HIGH);
        vTaskDelay(pdMS_TO_TICKS(200));
        digitalWrite(LED_ERROR1, LOW);
        digitalWrite(LED_ERROR2, LOW);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void ledHandler_showModbusError(bool enable)
{

    if (enable)
    {
        // digitalWrite(LED_ERROR1, HIGH);
        globalBitfield |= (1 << MODBUS_ERROR); // Set the third bit to 1
    }
    else
    {

        globalBitfield &= ~(1 << MODBUS_ERROR); // Clear the third bit to 0
                                                // digitalWrite(LED_ERROR1, LOW);
    }
}

void ledHandler_showCardReaderError(bool enable)
{
    if (enable)
    {
        // digitalWrite(LED_ERROR1, HIGH);
        globalBitfield |= (1 << CARD_READ_ERROR); // Set the third bit to 1
    }

    else
    {
        globalBitfield &= ~(1 << CARD_READ_ERROR); // Clear the third bit to 0
                                                   // digitalWrite(LED_ERROR1, LOW);
    }
}

void ledHandler_showNetworkError(bool enable)
{

    if (enable)
    {
        // digitalWrite(LED_ERROR1, HIGH);
        globalBitfield |= (1 << NETWORK_ERROR); // Set the third bit to 1
    }

    else
    {
        globalBitfield &= ~(1 << NETWORK_ERROR); // Clear the third bit to 0
        // digitalWrite(LED_ERROR1, LOW);
    }
}

void ledHandler_showTemperaturError(bool enable)
{

    if (enable)
    {
        globalBitfield |= (1 << TEMPERATUR_ERROR); // Set the third bit to 1

        // digitalWrite(LED_ERROR1, HIGH);
    }
    else
    {

        // digitalWrite(LED_ERROR1, LOW);
        globalBitfield &= ~(1 << TEMPERATUR_ERROR); // Clear the third bit to 0
    }
}

void ledHandler_showPWM(unsigned pwmValue)
{
    globalBlink = pwmValue;
}

unsigned ledHandler_getPWM()
{
    bool ledStatus = digitalRead(LED_ERROR2);
    TickType_t delayTicks;
    unsigned localBlink = globalBlink;
    if (localBlink <= 0)
    {
        ledStatus = LOW;
        digitalWrite(LED_ERROR2, ledStatus);
        delayTicks = 50;
    }
    else
    {
        ledStatus = !ledStatus;
        digitalWrite(LED_ERROR2, ledStatus);

        // 4. Dynamische Periode berechnen (0..254 gemappt auf 1500ms..50ms)
        // Höherer PWM-Wert = Kürzere Wartezeit = Schnelleres Blinken
        int intervallMs = map(localBlink, 1, 254, 1500, 50);

        // 5. Millisekunden in FreeRTOS-Ticks umrechnen
        delayTicks = pdMS_TO_TICKS(intervallMs);
    }
    return delayTicks;
}

/* void ledHandler_help_blink(byte bitErrorCode, uint8_t led)
{
    int status = 0;
    if ((globalBitfield & (1 << bitErrorCode)) != 0)
    {
        status = digitalRead(led);
        if (status == HIGH)
            digitalWrite(led, LOW);
        else
            digitalWrite(led, LOW);
    }
}
 */
uint8_t ledHandler_blink()
{
    bool ledStatus = digitalRead(LED_ERROR1);
    uint8_t lokalesBitfield = globalBitfield;
    int intervallMs = 0;
    if (lokalesBitfield == 0)
    {
        ledStatus = LOW;
        intervallMs=5000; // default wait for lookung for error
    }
    else
    {
        if (lokalesBitfield & (1 << MODBUS_ERROR))
        {
            intervallMs = 150; // Höchste Priorität: Extrem schnelles Blinken
        }
        else if (lokalesBitfield & (1 << TEMPERATUR_ERROR))
        {
            intervallMs = 400; // Mittlere Priorität: Schnelles Blinken
        }
        else if (lokalesBitfield & (1 << NETWORK_ERROR))
        {
            intervallMs = 1000; // Niedrige Priorität: Langsames Blinken
        }
        else if (lokalesBitfield & (1 << CARD_READ_ERROR))
        {
            intervallMs = 2000; // Niedrige Priorität: Langsames Blinken
        }
        ledStatus = !ledStatus;
    }
    digitalWrite(LED_ERROR1, ledStatus);

    return intervallMs;
}
