#include "debugConsole.h"
#include <Arduino.h>
#include "logReader.h"
#include "esp_log.h"

// ────────────────────────────────────────────────────
// Farben + Level-Char
// ────────────────────────────────────────────────────
#define LOG_COLOR_E "\033[0;31m" // Rot
#define LOG_COLOR_W "\033[0;33m" // Gelb
#define LOG_COLOR_I "\033[0;32m" // Gruen
#define LOG_COLOR_D ""
#define LOG_COLOR_V ""
#define LOG_RESET_COLOR "\033[0m"

// ────────────────────────────────────────────────────
// Konstanten
// ────────────────────────────────────────────────────
#ifndef IMMEDIATE_LOG_COUNT
#define IMMEDIATE_LOG_COUNT  4    // erste 4 Wiederholungen sofort
#endif

#ifndef SMART_LOG_INTERVAL
#define SMART_LOG_INTERVAL   5000 // ms bis zur „weitere"-Zusammenfassung
#endif

#define FORMAT_BUF_SIZE      512  // Groesse des format-Puffers
#define HASH_MAP_SIZE        64   // open-addressing Hash-Slots

// ────────────────────────────────────────────────────
// Per-Core statische Puffer (Vorschlag B)
//   Statt char buffer[512] auf dem Stack -> statisch pro Kern.
//   ESP32-S3 hat 2 Kerne -> 2 Buffer.
//   Stack-Sparen: ~512 Byte pro Log-Aufruf.
// ────────────────────────────────────────────────────
static char g_logBufA[FORMAT_BUF_SIZE];
static char g_logBufB[FORMAT_BUF_SIZE];

// ────────────────────────────────────────────────────
// Mutex fuer shouldLogSmart (Vorschlag C)
// ────────────────────────────────────────────────────
static SemaphoreHandle_t g_logMutex = nullptr;

// ────────────────────────────────────────────────────
// Hash-Eintrag (Vorschlag D: open-addressing statt linearem Scan)
// ────────────────────────────────────────────────────
struct LogEntry
{
    const char  *file;
    int          line;
    uint32_t     msgHash;
    int          count;
    uint32_t     lastLogTime;
};

static LogEntry hashMap[HASH_MAP_SIZE]; // 64 * 24 B = ~1.5 KB

static inline uint32_t slotKey(const char *file, int line)
{
    uint32_t h = (uint32_t)file;
    h = (h ^ (uint32_t)line) ^ (h >> 16);
    return (h & 0xFFFF) % HASH_MAP_SIZE;
}

static uint32_t calculateHash(const char *str)
{
    uint32_t hash = 2166136261u;
    while (*str) { hash ^= (uint32_t)*str++; hash *= 16777619u; }
    return hash;
}

// ── Helper ──────────────────────────────────────────
static inline const char *getLogColor(esp_log_level_t level)
{
    switch (level)
    {
    case ESP_LOG_ERROR:   return LOG_COLOR_E;
    case ESP_LOG_WARN:    return LOG_COLOR_W;
    case ESP_LOG_INFO:    return LOG_COLOR_I;
    case ESP_LOG_DEBUG:   return LOG_COLOR_D;
    case ESP_LOG_VERBOSE: return LOG_COLOR_V;
    default:              return "";
    }
}

static inline char getLevelChar(esp_log_level_t level)
{
    switch (level)
    {
    case ESP_LOG_ERROR:   return 'E';
    case ESP_LOG_WARN:    return 'W';
    case ESP_LOG_INFO:    return 'I';
    case ESP_LOG_DEBUG:   return 'D';
    case ESP_LOG_VERBOSE: return 'V';
    default:              return 'U';
    }
}

// ────────────────────────────────────────────────────
// shouldLogSmart (mutex-frei: wird nur mit g_logMutex aufgerufen)
//
//   open-addressing Hash-Map: O(1) lookup statt O(64) linearer Scan.
//   RETURN: true wenn die Message geschrieben werden darf.
// ────────────────────────────────────────────────────
static bool shouldLogSmart(const char *file, int line, const char *formattedMsg)
{
    uint32_t currentHash = calculateHash(formattedMsg);
    uint32_t now         = millis();

    uint32_t start = slotKey(file, line);
    uint32_t probe = start;

    for (uint32_t att = 0; att < HASH_MAP_SIZE; att++)
    {
        if (!hashMap[probe].file)
        {
            // Leer-Slot -> Eintrag anlegen
            hashMap[probe].file        = file;
            hashMap[probe].line        = line;
            hashMap[probe].msgHash     = currentHash;
            hashMap[probe].count       = 0;
            hashMap[probe].lastLogTime = now;
            return true;
        }
        if (hashMap[probe].file == file && hashMap[probe].line == line)
        {
            // Gleicher Hash -> Unterdruecken
            if (hashMap[probe].msgHash == currentHash)
            {
                hashMap[probe].count++;
                if (hashMap[probe].count <= IMMEDIATE_LOG_COUNT)
                {
                    hashMap[probe].lastLogTime = now;
                    return true;
                }
                if (now - hashMap[probe].lastLogTime >= SMART_LOG_INTERVAL)
                {
                    Serial.printf("--- [%s:%d] %d weitere Wiederholungen seit %lu ms ---\n",
                                  file, line,
                                  hashMap[probe].count - IMMEDIATE_LOG_COUNT,
                                  now - hashMap[probe].lastLogTime);
                    hashMap[probe].lastLogTime = now;
                    return true;
                }
                return false;
            }
            // Hash geaendert -> sofort loggen, Counter reset
            hashMap[probe].msgHash     = currentHash;
            hashMap[probe].count       = 0;
            hashMap[probe].lastLogTime = now;
            return true;
        }
        // Andere Datei/Zeile -> weiterproben
        probe = (probe + 1) % HASH_MAP_SIZE;
    }
    return true; // Map voll -> lieber loggen
}

// ────────────────────────────────────────────────────
// smartLogExec  (Vorschlag A + B + C + D)
//
//   A: Format-Buffer auf statischem Speicher (kein Stack-Druck)
//   B: Per-Core Buffer -> threadsicher ohne zusatz Mutex fuer Buffer
//   C: Mutex um shouldLogSmart fuer HashMap-Integritat
//   D: open-addressing Hash-Map -> O(1) Lookup
// ────────────────────────────────────────────────────
void smartLogExec(esp_log_level_t level, const char *tag,
                  const char *file, int line,
                  const char *format, ...)
{
    if (!g_logMutex)
    {
        g_logMutex = xSemaphoreCreateMutex();
        if (!g_logMutex) return;
    }

    // Per-Core statischer Buffer (kein Stack!)
    char *buf = (xPortGetCoreID() == 0) ? g_logBufA : g_logBufB;

    va_list args;
    va_start(args, format);
    int written = vsnprintf(buf, FORMAT_BUF_SIZE, format ? format : "", args);
    va_end(args);
    if (written < 0)
    {
        snprintf(buf, FORMAT_BUF_SIZE, "Logging Error: Invalid arguments or format");
    }

    // shouldLogSmart unter Mutex
    bool shouldPrint = false;
    xSemaphoreTake(g_logMutex, pdMS_TO_TICKS(10));
    shouldPrint = shouldLogSmart(file, line, buf);
    xSemaphoreGive(g_logMutex);

    // Schreiben nur wenn erlaubt
    if (shouldPrint)
    {
        esp_log_write(level, tag, "%s%c (%lu) %s: %s:%d | %s%s\n",
                      getLogColor(level),
                      getLevelChar(level),
                      (unsigned long)millis(),
                      tag, file, line,
                      buf, LOG_RESET_COLOR);
    }
}
