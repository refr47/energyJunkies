#pragma once

#include <Arduino.h>
#include <esp_log.h>
#include <time.h>
#include <string.h>
#include "mqtt.h"
#define TEXTIFY(A) #A
#define ESCAPEQUOTE(A) TEXTIFY(A)

#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT

#ifndef DEBUG_PORT
#define DEBUG_PORT Serial
#endif

//int debug_LogOutput(const char *format, va_list args);
void smartLogExec(esp_log_level_t level, const char *tag, const char *file, int line, const char *format, ...);
bool shouldLogSmart(const char *file, int line, const char *formattedMsg);

#define FINFO strrchr(__FILE__, '/') + 1

#define DBGbgn(speed) Serial.begin(speed)

#define DBGf(M, ...) ESP_LOGD("APP", "%s:%d | " M, __FILE__, __LINE__, ##__VA_ARGS__)

/*
ESP_LOGE - error (lowest)

ESP_LOGW - warning

ESP_LOGI - info

ESP_LOGD - debug

ESP_LOGV - verbose (highest)

*/
 
  
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

#define LOG_ERROR(tag, M, ...) smartLogExec(ESP_LOG_ERROR, tag, __FILE__, __LINE__, M, ##__VA_ARGS__)
#define LOG_INFO(tag, M, ...) smartLogExec(ESP_LOG_INFO, tag, __FILE__, __LINE__, M, ##__VA_ARGS__)
#define LOG_DEBUG(tag, M, ...) smartLogExec(ESP_LOG_DEBUG, tag, __FILE__, __LINE__, M, ##__VA_ARGS__)
#define LOG_WARNING(tag, M, ...) smartLogExec(ESP_LOG_WARN, tag, __FILE__, __LINE__, M, ##__VA_ARGS__)
//#define LOG_VERBOSE(tag,M, ...) ESP_LOGV(tag, "%s:%d | " M, __FILE__, __LINE__, ##__VA_ARGS__)

#define DBG(M, ...) smartLogExec(ESP_LOG_DEBUG, "*", "%s:%d | " M, __FILE__, __LINE__, ##__VA_ARGS__)

#else
#define DBGbgn(speed)
#define DBGf(format, ...)
#define DBG(format, ...)

#endif

    /*

    // 1. Die Callback-Funktion definieren
    vprintf_like_t original_vprintf = NULL;

    int mqtt_vprintf_hook(const char *format, va_list args) {
        // 1. Normal auf Serial ausgeben (originale Funktion)
        int written = original_vprintf(format, args);

        // 2. Optional: Log an MQTT senden
        // Aber VORSICHT: Nicht loggen, wenn wir gerade im MQTT-Task sind (Endlosschleife!)
        // Wir verwenden einen statischen Buffer, um Stack zu sparen
        static char msg_buffer[256];
        vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);

        // Hier dein MQTT Publish (nur wenn Netzwerk da ist)
        // mqtt_publish_log('L', msg_buffer);

        return written;
    }

    */