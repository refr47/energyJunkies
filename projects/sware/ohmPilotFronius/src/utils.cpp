#define __UTILS_CPP__
#include "utils.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

// using namespace std; // im lazy

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

String ipv4_int_to_string(uint32_t in, bool *const success)
{
    char ret[INET_ADDRSTRLEN];
    in = htonl(in);
    const bool _success = (NULL != inet_ntop(AF_INET, &in, &ret[0], INET_ADDRSTRLEN));
    if (success)
    {
        *success = _success;
    }
    /* Serial.print("Return in ipv4_int_to_string ...");
    Serial.print(", success: ");Serial.print(_success);Serial.print(" ,ret: "); Serial.print(ret); */
    if (_success)
    {
        // ret.pop_back(); // remove null-terminator required by inet_ntop
    }
    else
    {
        char buf[200] = {0};
        strerror_r(errno, buf, sizeof(buf));
        Serial.print("Error inipv4_int_to_string ");
        Serial.println(strerror(errno));

        // throw std::runtime_error(String("error converting ipv4 int to String ") + to_string(errno) + String(": ") + String(buf));
        // ret = buf;
    }

    Serial.println("===");
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
        char buf[200] = {0};
        strerror_r(errno, buf, sizeof(buf));
        Serial.print("Error in ipv4_string_to_int ");
        Serial.println(strerror(errno));
        in = buf;
        // throw std::runtime_error(String("error converting ipv4 String to int ") + to_string(errno) + String(": ") + String(buf));
    }
    return ret;
}
bool util_isFieldFilled(const char *key, const char *argument, StaticJsonDocument<100> &data)
{
    if (strlen(argument) == 0)
    {
        char buf[50];
        sprintf(buf, "Argument: %s kann nicht leer sein.", key);
        data["error"] = buf;
        return false;
    }
    return true;
}

bool util_checkParamInt(const char *key, const char *argument, /*const JsonObject &jsonObj, */StaticJsonDocument<100> &data, int *result)
{
    if (util_isFieldFilled(key, argument, data))
        *result = atoi(argument);
    else
        return false;
    if (*result == 0)
    {
        char buf[50];
        sprintf(buf, "Argument: %s ist kein numerischer Wert.", key);
        data["error"] = buf;
        return false;
    }
    return true;
}

bool util_checkParamFloat(const char *key, const char *argument,/* const JsonObject &jsonObj, */StaticJsonDocument<100> &data, float *result)
{
    if (util_isFieldFilled(key, argument, data))
        *result = atof(argument);
    else
        return false;
    if (*result == 0.0)
    {
        char buf[50];
        sprintf(buf, "Argument: %s ist kein numerischer Wert.", key);
        data["error"] = buf;
        return false;
    }
    return true;
}