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

void printHWInfo()
{
    Serial.print("MEM: ");
    Serial.println(esp_get_free_heap_size());

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    Serial.println("Hardware info");
    Serial.printf("%d cores Wifi %s%s\n", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                  (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    Serial.printf("Silicon revision: %d\n", chip_info.revision);

    // get chip id
    uint32_t chipId = ESP.getEfuseMac();

    Serial.printf("Chip id: %x\n", chipId);
}
bool isNumber(char s[])
{
    for (int i = 0; s[i] != '\0'; i++)
    {
        if (isdigit(s[i]) == 0)
            return false;
    }
    return true;
}

bool floatNum(char *s)
{
    const char *ptr = s;
    double x = strtod(ptr, &s);

    // check if converted to long int
    if (*s == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
    return false;
}