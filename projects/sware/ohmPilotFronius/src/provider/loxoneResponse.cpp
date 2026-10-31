#ifdef LOXONE_API
#include "loxoneResponse.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <cctype>

namespace {
bool number(JsonVariantConst input, double &value)
{
    if (input.is<double>()) {
        value = input.as<double>();
        return std::isfinite(value);
    }
    if (!input.is<const char*>()) return false;
    const char* text = input.as<const char*>();
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(text, &end);
    if (end == text || errno == ERANGE || !std::isfinite(parsed)) return false;
    while (std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return false;
    value = parsed;
    return true;
}
}

bool loxoneParseValue(const char* payload, double &value)
{
    JsonDocument doc;
    if (!payload || deserializeJson(doc, payload)) return false;
    double code = 0, parsed = 0;
    if (!number(doc["LL"]["Code"], code) || code != 200 ||
        !number(doc["LL"]["value"], parsed)) return false;
    value = parsed;
    return true;
}
#endif
