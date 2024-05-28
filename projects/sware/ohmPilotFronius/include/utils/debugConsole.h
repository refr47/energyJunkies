#pragma once
#include <Arduino.h>
#include <time.h>
#include <string.h>

#define TEXTIFY(A) #A
#define ESCAPEQUOTE(A) TEXTIFY(A)

#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT

#ifndef DEBUG_PORT
#define DEBUG_PORT Serial
#endif
int debug_LogOutput(const char *format, va_list args);
// This only works for C
/* static const char *currTime()
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char *thetime = asctime(timeinfo);
    thetime[strlen(thetime) - 5] = '\0';

    return (const char *)(thetime + 10);
}
// What is the current time
#define DATE_STRING currTime() */

#define FINFO strrchr(__FILE__, '/') + 1

#define DBGbgn(speed) Serial.begin(speed)
/*
#define DBGf(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)

#define DBG(format, ...) DEBUG_PORT.printf(format, ##__VA_ARGS__) */

#define DBGf(M, ...) DEBUG_PORT.printf("%s:%d | " M "\n", FINFO, __LINE__, ##__VA_ARGS__)

/*
ESP_LOGE - error (lowest)

ESP_LOGW - warning

ESP_LOGI - info

ESP_LOGD - debug

ESP_LOGV - verbose (highest)

*/

// esp_log_write(ESP_LOG_ERROR, "", format, ##__VA_ARGS__)

#define LOG_ERROR(M, ...) esp_log_write(ESP_LOG_ERROR, "", "%s:%d | " M "\n", FINFO, __LINE__, ##__VA_ARGS__)

#define LOG_WARNING(M, ...) ESP_LOGW(TAG, "%s:%d | " M "\n", FINFO, __LINE__, ##__VA_ARGS__)

#define LOG_INFO(M, ...) esp_log_write(ESP_LOG_INFO, "", "%s:%d > " M, FINFO, __LINE__, ##__VA_ARGS__)

#define LOG_DEBUG(M, ...) ESP_LOGD(TAG, "%s:%d > " M, FINFO, __LINE__, ##__VA_ARGS__)

#define LOG_VERBOSE(M, ...) ESP_LOGV(TAG, "%s:%d | " M "\n", FINFO, __LINE__, ##__VA_ARGS__)

#define DBG(M, ...) DEBUG_PORT.printf("%s:%d | " M, FINFO, __LINE__, ##__VA_ARGS__)

/* #define DBGln(a) (Serial.println(a))
#define DBG1ln(a, f) (Serial.println(a, f))

#define DBG(a) (Serial.print(a))
#define DBG1(a, f) (Serial.print(a, f)) */
#else
#define DBGbgn(speed)
#define DBGf(format, ...)
#define DBG(format, ...)

#endif