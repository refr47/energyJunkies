#define __pidReglerCPP

#include <Arduino.h>
#include <PID_v1.h>
#include "pidRegler.h"
#include "pin_config.h"

#define MIN_VAL 0
#define MAX_VAL 255

static Setup *_setup;

bool pid_init(Setup *s)
{
    _setup = s;
    pinMode(PID_REGLER_GPIO, OUTPUT); // declare pwm pin to be an output:
    return true;
}

void pid_run(float availableOutputFromInverterInWatt)
{
    for (int jj = MIN_VAL; jj < MAX_VAL; jj++)
    {
        analogWrite(PID_REGLER_GPIO, jj); // set the brightness of led
        delay(500);
    }
    for (int jj = MAX_VAL; jj > MIN_VAL; jj--)
    {
        analogWrite(PID_REGLER_GPIO, jj); // set the brightness of led
        delay(500);
    }
}