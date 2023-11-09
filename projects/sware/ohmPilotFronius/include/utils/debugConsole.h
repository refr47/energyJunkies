#pragma once
#include <Arduino.h>


#define TEXTIFY(A) #A
#define ESCAPEQUOTE(A) TEXTIFY(A)

/*

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_NONE
#ifndef USE_ESP_IDF_LOG
#define log_n(format, ...) log_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define isr_log_n(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define log_buf_n(b,l) do{ARDUHAL_LOG_COLOR_PRINT(E);log_print_buf(b,l);ARDUHAL_LOG_COLOR_PRINT_END;}while(0)
#else
#define log_n(format, ...) do {ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, TAG, format, ##__VA_ARGS__);}while(0)
#define isr_log_n(format, ...) do {ets_printf(LOG_FORMAT(E, format), esp_log_timestamp(), TAG, ##__VA_ARGS__);}while(0)
#define log_buf_n(b,l) do {ESP_LOG_BUFFER_HEXDUMP(TAG, b, l, ESP_LOG_ERROR);}while(0)
#endif
#else
#define log_n(format, ...) do {} while(0)
#define isr_log_n(format, ...) do {} while(0)
#define log_buf_n(b,l) do {} while(0)
#endif
*/




#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT

#ifndef DEBUG_PORT
#define DEBUG_PORT Serial
#endif

#define DBGbgn(speed) Serial.begin(speed)

#define DBGf(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)
#define DBG(...) DEBUG_PORT.print(__VA_ARGS__)
#define DBGln(...) DEBUG_PORT.println(__VA_ARGS__)
#define DBG1(a, f) (Serial.print(a, f))

/* #define DBGln(a) (Serial.println(a))
#define DBG1ln(a, f) (Serial.println(a, f))

#define DBG(a) (Serial.print(a))
#define DBG1(a, f) (Serial.print(a, f)) */
#else
#define DBGln(a)
#define DBG(a)
#define DBG1ln(a, f)
#define DBG1(a, f)
#endif