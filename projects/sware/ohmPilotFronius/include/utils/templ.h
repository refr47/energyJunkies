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

// Booleans: akzeptiert JSON true/false (bool) UND JSON 0/1 (int)
// ArduinoJson is<bool>() ist inkonsistent → wir prüfen beides.
template <typename Struct>
struct BooleanField : FieldBase<Struct>
{
    const char *key;
    bool Struct::*target;

    BooleanField(const char *k, bool Struct::*t) : key(k), target(t) {}

    const char *getKey() const override { return key; }
    bool update(Struct &obj, JsonVariant value) const override
    {
        if (value.isNull())
        {
            return false;
        }

        bool b = false;
        if (value.is<bool>())
        {
            b = value.as<bool>();
        }
        else if (value.is<int>())
        {
            b = (value.as<int>() != 0);
        }
        else
        {
            return false;
        }

        obj.*target = b;
        return true;
    }
    void serialize(const Struct &obj, JsonObject &json) const override
    {
        json[key] = obj.*target;
    }
};

// Für alle numerischen Typen (int, unsigned int, short, double)
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

// Für char-Arrays (Strings) — direkter Member-Zugriff
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

        if (str == nullptr)
            return false;

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

// Lambda-basierter StringField für verschachtelte Members
// Zugriff ueber Funktoren statt Pointer-to-Member
template <typename Struct, size_t N>
struct LambdaStringField : FieldBase<Struct>
{
    const char *key;
    const char * (*_get)(const Struct &obj);
    void (*_set)(Struct &obj, const char *str);

    LambdaStringField(const char *k, const char * (*g)(const Struct &), void (*s)(Struct &, const char *))
        : key(k), _get(g), _set(s) {}

    const char *getKey() const override { return key; }
    bool update(Struct &obj, JsonVariant value) const override
    {
        if (value.isNull() || !value.is<const char *>())
            return false;

        const char *str = value.as<const char *>();
        if (str == nullptr)
            return false;

        if (strlen(str) >= N)
            return false;

        _set(obj, str);
        return true;
    }
    void serialize(const Struct &obj, JsonObject &json) const override
    {
        json[key] = _get(obj);
    }
};

// Lambda-basierter NumericField fuer verschachtelte Members
template <typename T, typename Struct>
struct LambdaNumericField : FieldBase<Struct>
{
    const char *key;
    T (*_get)(const Struct &obj);
    void (*_set)(Struct &obj, T value);

    LambdaNumericField(const char *k, T (*g)(const Struct &), void (*s)(Struct &, T))
        : key(k), _get(g), _set(s) {}

    const char *getKey() const override { return key; }
    bool update(Struct &obj, JsonVariant value) const override
    {
        if (value.isNull() || !value.is<T>())
            return false;

        _set(obj, value.as<T>());
        return true;
    }
    void serialize(const Struct &obj, JsonObject &json) const override
    {
        json[key] = _get(obj);
    }
};

// Lambda-basierter BooleanField fuer verschachtelte Members
template <typename Struct>
struct LambdaBooleanField : FieldBase<Struct>
{
    const char *key;
    bool (*_get)(const Struct &obj);
    void (*_set)(Struct &obj, bool val);

    LambdaBooleanField(const char *k, bool (*g)(const Struct &), void (*s)(Struct &, bool))
        : key(k), _get(g), _set(s) {}

    const char *getKey() const override { return key; }
    bool update(Struct &obj, JsonVariant value) const override
    {
        if (value.isNull())
            return false;

        bool b = false;
        if (value.is<bool>())
            b = value.as<bool>();
        else if (value.is<int>())
            b = (value.as<int>() != 0);
        else
            return false;

        _set(obj, b);
        return true;
    }
    void serialize(const Struct &obj, JsonObject &json) const override
    {
        json[key] = _get(obj);
    }
};