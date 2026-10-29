#pragma once

#include <Arduino.h>
#include <esp_log.h>
#include <time.h>
#include <string.h>
#include "mqtt.h"
#define TEXTIFY(A) #A
#define ESCAPEQUOTE(A) TEXTIFY(A)

#define DEBUG_OUTPUT

// ─────────────────────────────────────────────────────
// Runtime-Log-Level (compile-time): 0=error, 1=warn, 2=info, 3=debug
//  Auf 2 setzen, um LOG_DEBUG komplett abzuschalten (kein Stack-Kosten!)
// ─────────────────────────────────────────────────────
#ifndef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL 3
#endif

#ifdef DEBUG_OUTPUT

#ifndef DEBUG_PORT
#define DEBUG_PORT Serial
#endif

void smartLogExec(esp_log_level_t level, const char* tag, const char* file, int line, const char* format, ...);

#define FINFO strrchr(__FILE__, '/') + 1

#define DBGbgn(speed) Serial.begin(speed)

// ESP_LOGD nur durch eigene smartLogExec ersetzen
#define DBGf(M, ...) ESP_LOGD("APP", "%s:%d | " M, __FILE__, __LINE__, ##__VA_ARGS__)

#define TAG_MAIN "APP_MAIN"
#define TAG_WLAN "WLAN"
#define TAG_TEMP "TEMP"
#define TAG_PID "PID_CONTROL"
#define TAG_MQTT "MQTT"
#define TAG_WEB "WEB"
#define TAG_APP_TASKS "APP_TASKS"
#define TAG_SHELLY "SHELLY"
#define TAG_CARD "CARD"
#define TAG_MODBUS "MODBUS"
#define TAG_FRONIUS "FRONIUS"
#define TAG_AMIS "AMIS"
#define TAG_LOXONE "LOXONE"
#define TAG_WEATHER "WEATHER"
#define TAG_INFLUX "INFLUX"
#define TAG_APP_SERVICES "APP_SERVICE"
#define TAG_WEB_SOCKETS "WEB_SOCKETS"
#define TAG_AJAX "AJAX"
#define TAG_EPPROM "EPROM"
#define TAG_SOLAR "SOLAR"
#define TAG_HOT_UPDATE "HOT_UPDATE"
#define TAG_UTILS "UTILS"
#define TAG_TIME    "TIME"
#define TAG_LOGGER "LOGGER"
#define TAG_TFT "TFT"
#define TAG_PHASE "PHASE"
#define TAG_BATT    "BATTERY"
#define TAG_ML      "ML_PRED"
#define TAG_THERMAL "THERMAL"
#define TAG_TINYNN  "TINYNN"
#define TAG_BATT    "AKKU"
#define TAG_FRONIUS      "FRONIUS"

/* ── Level-Gate: wenn MIN_LOG_LEVEL < Log-Level, wird gar nichts gemacht ──
   Kein Stack-Frame, kein va_list, kein Mutex, keine Formatierung.       */
#define LOG_ERROR(tag, M, ...) \
    do { if (MIN_LOG_LEVEL >= ESP_LOG_ERROR)  smartLogExec(ESP_LOG_ERROR,  tag, __FILE__, __LINE__, M, ##__VA_ARGS__); } while(0)
#define LOG_WARNING(tag, M, ...) \
    do { if (MIN_LOG_LEVEL >= ESP_LOG_WARN)   smartLogExec(ESP_LOG_WARN,   tag, __FILE__, __LINE__, M, ##__VA_ARGS__); } while(0)
#define LOG_INFO(tag, M, ...) \
    do { if (MIN_LOG_LEVEL >= ESP_LOG_INFO)   smartLogExec(ESP_LOG_INFO,   tag, __FILE__, __LINE__, M, ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(tag, M, ...) \
    do { if (MIN_LOG_LEVEL >= ESP_LOG_DEBUG)  smartLogExec(ESP_LOG_DEBUG,  tag, __FILE__, __LINE__, M, ##__VA_ARGS__); } while(0)

#define DBG(M, ...) \
    do { if (MIN_LOG_LEVEL >= ESP_LOG_DEBUG)  smartLogExec(ESP_LOG_DEBUG,  "*", M, ##__VA_ARGS__); } while(0)

#else  // DEBUG_OUTPUT aus
#define DBGbgn(speed)
#define DBGf(format, ...)
#define DBG(format, ...)
#define LOG_ERROR(tag, M, ...)
#define LOG_WARNING(tag, M, ...)
#define LOG_INFO(tag, M, ...)
#define LOG_DEBUG(tag, M, ...)
#endif
