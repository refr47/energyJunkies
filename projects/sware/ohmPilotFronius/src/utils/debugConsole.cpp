#include "debugConsole.h"
#include <Arduino.h>
#include "logReader.h"
#include "esp_log.h"

#define LOG_COLOR_E "\033[0;31m" // Rot
#define LOG_COLOR_W "\033[0;33m" // Gelb
#define LOG_COLOR_I "\033[0;32m" // Grün
#define LOG_COLOR_D ""           // Standard (Weiß/Grau)
#define LOG_COLOR_V ""           // Standard
#define LOG_RESET_COLOR "\033[0m"

#define LOG_BUFFER_SIZE 64 // Reicht meist für die kritischen Zeilen in Loops

typedef struct _LogEntry
{
    const char *file;
    int line;
    uint32_t lastHash;
    uint32_t count;
} LogEntry;

static uint32_t calculateHash(const char *str);

static int nextBufferIdx = 0;

static EXT_RAM_ATTR LogEntry logBuffer[LOG_BUFFER_SIZE];

/*
HILFSfunktionen für die smarte Logik
*/
static inline const char *getLogColor(esp_log_level_t level)
{
    switch (level)
    {
    case ESP_LOG_ERROR:
        return LOG_COLOR_E;
    case ESP_LOG_WARN:
        return LOG_COLOR_W;
    case ESP_LOG_INFO:
        return LOG_COLOR_I;
    case ESP_LOG_DEBUG:
        return LOG_COLOR_D;
    case ESP_LOG_VERBOSE:
        return LOG_COLOR_V;
    default:
        return "";
    }
}
static inline char getLevelChar(esp_log_level_t level)
{
    switch (level)
    {
    case ESP_LOG_ERROR:
        return 'E';
    case ESP_LOG_WARN:
        return 'W';
    case ESP_LOG_INFO:
        return 'I';
    case ESP_LOG_DEBUG:
        return 'D';
    case ESP_LOG_VERBOSE:
        return 'V';
    default:
        return 'U'; // Unknown
    }
}
// Eine sehr schnelle Hash-Funktion (FNV-1a)
static uint32_t calculateHash(const char *str)
{
    uint32_t hash = 2166136261u;
    while (*str)
    {
        hash ^= (uint32_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

bool shouldLogSmart(const char *file, int line, const char *formattedMsg)
{
    uint32_t currentHash = calculateHash(formattedMsg);
    int foundIdx = -1;

    // 1. Suchen, ob diese Zeile schon im Buffer ist
    for (int i = 0; i < LOG_BUFFER_SIZE; i++)
    {
        if (logBuffer[i].file == file && logBuffer[i].line == line)
        {
            foundIdx = i;
            break;
        }
    }

    // 2. Logik: Ist es neu oder hat sich der Wert geändert?
    if (foundIdx != -1)
    {
        if (logBuffer[foundIdx].lastHash == currentHash)
        {
            logBuffer[foundIdx].count++;
            return false; // Inhalt identisch -> Nicht loggen
        }
        else
        {
            // Wert hat sich geändert!
            if (logBuffer[foundIdx].count > 0)
            {
                Serial.printf("--- [%s:%d] %d mal wiederholt ---\n", file, line, logBuffer[foundIdx].count);
            }
            logBuffer[foundIdx].lastHash = currentHash;
            logBuffer[foundIdx].count = 0;
            return true;
        }
    }

    // 3. Neuer Eintrag im Ringbuffer (Überschreiben nach FIFO)
    logBuffer[nextBufferIdx] = {file, line, currentHash, 0};
    nextBufferIdx = (nextBufferIdx + 1) % LOG_BUFFER_SIZE;
    return true;
}


void smartLogExec(esp_log_level_t level, const char *tag, const char *file, int line, const char *format, ...)
{
    static char buffer[256]; // Puffer für die formatierte Nachricht
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (shouldLogSmart(file, line, buffer))
    {
        esp_log_write(level, tag, "%s%c (%lu) %s: %s:%d | %s%s\n",
                      getLogColor(level),
                      getLevelChar(level),
                      (unsigned long)millis(),
                      tag,
                      file,
                      line,
                      buffer,
                      LOG_RESET_COLOR);
    }
}
/* static char buf[200];
int debug_LogOutput(const char *format, va_list args)
{

    int ret = vsnprintf(buf, sizeof(buf), format, args);
    logReader_captureSerialOutput((const char *)buf);
    Serial.printf("%s\n", buf);
    // DBGf("%s", buf);
    return ret;
} */
