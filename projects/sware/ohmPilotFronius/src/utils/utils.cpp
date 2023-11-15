#define __UTILS_CPP__
#include "utils.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <Arduino.h>

// using namespace std; // im lazy
#define BUFFER_LEN_FOR_ARG_CHECK 100
/* void Serialprintln(const char *input...)
{
    va_list args;
    va_start(args, input);

    for (const char *i = input; *i != 0; ++i)
    {
        if (*i != '%')
        {
            DBG(*i);
            continue;
        }
        switch (*(++i))
        {
        case '%':
            DBG('%');
            break;
        case 's':
            DBG(va_arg(args, char *));
            break;
        case 'd':
            Serial.println(va_arg(args, int), DEC);
            break;
        case 'b':
            Serial.println(va_arg(args, int), BIN);
            break;
        case 'o':
            Serial.println(va_arg(args, int), OCT);
            break;
        case 'x':
            Serial.println(va_arg(args, int), HEX);
            break;
        case 'f':
            Serial.println(va_arg(args, double), 2);
            break;
        }
    }
    DBGln();
    va_end(args);
} */

void printHWInfo()
{
    DBGf("MEM: %d", esp_get_free_heap_size());

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    DBGf("Hardware info: %d cores Wifi %s%s\n", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    DBGf("Silicon revision: %d\n", chip_info.revision);

    // get chip id
    uint32_t chipId = ESP.getEfuseMac();

    DBGf("Chip id: %x\n", chipId);
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

String ipv4_int_to_string(uint32_t in, bool *const success)
{
    char ret[INET_ADDRSTRLEN];
    in = htonl(in);
    const bool _success = (NULL != inet_ntop(AF_INET, &in, &ret[0], INET_ADDRSTRLEN));
    if (success)
    {
        *success = _success;
    }
    /* DBG("Return in ipv4_int_to_string ...");
    DBG(", success: ");DBG(_success);DBG(" ,ret: "); DBG(ret); */
    if (_success)
    {
        // ret.pop_back(); // remove null-terminator required by inet_ntop
    }
    else
    {
        char buf[BUFFER_LEN_FOR_ARG_CHECK] = {0};
        strerror_r(errno, buf, sizeof(buf));
        DBGf("Error inipv4_int_to_string  %s", strerror(errno));

        // throw std::runtime_error(String("error converting ipv4 int to String ") + to_string(errno) + String(": ") + String(buf));
        // ret = buf;
    }

    return String(ret);
}
// return is native-endian
// when an error occurs: if success ptr is given, it's set to false, otherwise a std::runtime_error is thrown.
uint32_t ipv4_string_to_int(String &in, bool *const success)
{
    uint32_t ret;
    const bool _success = (1 == inet_pton(AF_INET, in.c_str(), &ret));
    ret = ntohl(ret);
    if (success)
    {
        *success = _success;
    }
    else if (!_success)
    {
        char buf[BUFFER_LEN_FOR_ARG_CHECK] = {0};
        strerror_r(errno, buf, sizeof(buf));
        DBGf("Error in ipv4_string_to_int %s", strerror(errno));

        in = buf;
        // throw std::runtime_error(String("error converting ipv4 String to int ") + to_string(errno) + String(": ") + String(buf));
    }
    return ret;
}
bool util_isFieldFilled(const char *key, const char *argument, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data)
{
    if (strlen(argument) == 0)
    {
        char buf[BUFFER_LEN_FOR_ARG_CHECK];
        sprintf(buf, "Argument: %s kann nicht leer sein.", key);
        data["error"] = buf;
        DBGf("util_isFieldFilled: %s - empty!", key);

        return false;
    }
    return true;
}

bool util_checkParamInt(const char *key, const char *argument, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, int *result)
{
    if (util_isFieldFilled(key, argument, data))
        *result = atoi(argument);
    else
    {
        DBGf("utilCheckParamInt:  %s - empty", key);

        return false;
    }

    if (*result == 0)
    {
        char buf[BUFFER_LEN_FOR_ARG_CHECK];
        sprintf(buf, "Argument: %s ist kein numerischer Wert.", key);
        data["error"] = buf;
        DBGf("utilCheckParamInt: %s - kein numerischer Wert : %s", key, argument);

        return false;
    }
    return true;
}

bool util_checkParamFloat(const char *key, const char *argument, /* const JsonObject &jsonObj, */ StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, float *result)
{
    if (util_isFieldFilled(key, argument, data))
        *result = atof(argument);
    else
    {
        DBGf("util_checkParamFloat: %s - empty", key);

        return false;
    }
    if (*result == 0.0)
    {
        char buf[BUFFER_LEN_FOR_ARG_CHECK];
        sprintf(buf, "Argument: %s ist kein FLießkommawert (z.B. 0.0,...)", key);
        data["error"] = buf;
        DBGf("util_checkParamFloat: %s - kein numerischer Werte: %s", key, argument);

        return false;
    }
    return true;
}

void util_pHW()
{
    esp_chip_info_t chip_info;

    esp_chip_info(&chip_info);

    DBGf("Hardware info");
    Serial.printf("%d cores Wifi %s%s\n", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                  (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    Serial.printf("Silicon revision: %d\n", chip_info.revision);
    Serial.printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
                  (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embeded" : "external");

    // get chip id
    String chipId = String((uint32_t)ESP.getEfuseMac(), HEX);
    chipId.toUpperCase();

    DBGf("Chip id: %s\n", chipId.c_str());
}

char   *util_format_Watt_kWatt(double val,char *formatBuf) {
    if (fabs(val) > 1000.0)
        sprintf(formatBuf,"%.2lf kW",val/1000);
    else
         sprintf(formatBuf,"%.2lf W",val);
    return formatBuf;
}