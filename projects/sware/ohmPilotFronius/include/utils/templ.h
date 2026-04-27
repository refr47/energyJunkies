#pragma once

#include <Arduino.h>
#include <AsyncJson.h>
#include "debugConsole.h"
#include <memory>

template <typename T, typename... Args>
std::unique_ptr<T> my_make_unique(Args &&...args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
// Basisklasse
template <typename Struct>
struct FieldBase
{ // 'struct' macht alles public
    virtual const char *getKey() const = 0;
    virtual bool update(Struct &obj, JsonVariant value) const = 0;
    // Serialisierung von Struktur nach JSON
    virtual void serialize(const Struct &obj, JsonObject &json) const = 0;
    virtual ~FieldBase() {}
};

// Für alle numerischen Typen (int, unsigned int, short, double, bool)
template <typename T, typename Struct>
struct NumericField : FieldBase<Struct>
{
    const char *key;
    T Struct::*target;

    NumericField(const char *k, T Struct::*t) : key(k), target(t) {}

    const char *getKey() const override { return key; }
    bool update(Struct &obj, JsonVariant value) const override
    {
        if (value.isNull() || !value.is<T>()) // is
        {
            return false;
        }

        obj.*target = value.as<T>();
        return true;
    }
    void serialize(const Struct &obj, JsonObject &json) const override
    {
        json[key] = obj.*target;
    }
};

// Für char-Arrays (Strings)
template <typename Struct, size_t N>
struct StringField : FieldBase<Struct>
{
    const char *key;
    char (Struct::*target)[N];

    StringField(const char *k, char (Struct::*t)[N]) : key(k), target(t) {}

    const char *getKey() const override { return key; }
    bool update(Struct &obj, JsonVariant value) const override
    {
        if (value.isNull() || !value.is<const char *>())
        {
            return false;
        }
        const char *str = value.as<const char *>();

        // 2. Zusätzliche Sicherheit gegen Null-Pointer (doppelt hält besser)
        if (str == nullptr)
            return false;

        // 3. Längenprüfung
        if (strlen(str) >= N)
            return false;

        strlcpy(obj.*target, value.as<const char *>(), N);
        return true;
    }
    void serialize(const Struct &obj, JsonObject &json) const override
    {
        json[key] = (const char *)(obj.*target);
    }
};