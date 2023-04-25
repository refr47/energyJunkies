#define __UTILS_CPP__

#include <stdarg.h>
#include <Arduino.h>
#include "utils.h"

void Serialprintln(const char *input...)
{
    va_list args;
    va_start(args, input);

    for (const char *i = input; *i != 0; ++i)
    {
        if (*i != '%')
        {
            Serial.print(*i);
            continue;
        }
        switch (*(++i))
        {
        case '%':
            Serial.print('%');
            break;
        case 's':
            Serial.print(va_arg(args, char *));
            break;
        case 'd':
            Serial.print(va_arg(args, int), DEC);
            break;
        case 'b':
            Serial.print(va_arg(args, int), BIN);
            break;
        case 'o':
            Serial.print(va_arg(args, int), OCT);
            break;
        case 'x':
            Serial.print(va_arg(args, int), HEX);
            break;
        case 'f':
            Serial.print(va_arg(args, double), 2);
            break;
        }
    }
    Serial.println();
    va_end(args);
}