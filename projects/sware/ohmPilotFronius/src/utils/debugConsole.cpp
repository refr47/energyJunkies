#include "debugConsole.h"
#include <Arduino.h>
#include "logReader.h"
#include "esp_log.h"
#include <WiFi.h>
#include <lwip/sockets.h>
#include <errno.h>

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

// ESP logging may run on either core. Each callback owns one fixed buffer
// until the existing loop() has sent or discarded it. No large stack buffer
// and no socket call are needed in the logging callback.
static constexpr uint8_t NET_LOG_SLOTS = 8;
static constexpr size_t NET_LOG_LINE_SIZE = 384;
static constexpr uint16_t NET_LOG_PORT = 4444;
static char g_netLines[NET_LOG_SLOTS][NET_LOG_LINE_SIZE];
static uint16_t g_netLineLengths[NET_LOG_SLOTS];
static QueueHandle_t g_netFreeSlots = nullptr;
static QueueHandle_t g_netPending = nullptr;
static vprintf_like_t g_previousLogOutput = nullptr;
static WiFiServer g_netLogServer(NET_LOG_PORT, 1);
static WiFiClient g_netLogClient;
static int g_netActiveSlot = -1;
static size_t g_netActiveOffset = 0;
static bool g_netServerStarted = false;

static int mirrorLogOutput(const char *format, va_list args)
{
    uint8_t slot;
    if (g_netFreeSlots && xQueueReceive(g_netFreeSlots, &slot, 0) == pdTRUE)
    {
        va_list copy;
        va_copy(copy, args);
        const int length = vsnprintf(g_netLines[slot], NET_LOG_LINE_SIZE, format, copy);
        va_end(copy);
        if (length > 0)
        {
            g_netLineLengths[slot] = static_cast<uint16_t>(
                length < static_cast<int>(NET_LOG_LINE_SIZE) ? length : NET_LOG_LINE_SIZE - 1);
            if (xQueueSend(g_netPending, &slot, 0) == pdTRUE)
                return g_previousLogOutput(format, args);
        }
        xQueueSend(g_netFreeSlots, &slot, 0);
    }
    return g_previousLogOutput(format, args);
}

void debugConsole_initNetwork()
{
    if (g_previousLogOutput) return;
    g_netFreeSlots = xQueueCreate(NET_LOG_SLOTS, sizeof(uint8_t));
    g_netPending = xQueueCreate(NET_LOG_SLOTS, sizeof(uint8_t));
    if (!g_netFreeSlots || !g_netPending)
    {
        if (g_netFreeSlots) vQueueDelete(g_netFreeSlots);
        if (g_netPending) vQueueDelete(g_netPending);
        g_netFreeSlots = nullptr;
        g_netPending = nullptr;
        return;
    }
    for (uint8_t slot = 0; slot < NET_LOG_SLOTS; ++slot)
        xQueueSend(g_netFreeSlots, &slot, 0);
    g_previousLogOutput = esp_log_set_vprintf(mirrorLogOutput);
}

static void releaseNetworkLogSlot(uint8_t slot)
{
    xQueueSend(g_netFreeSlots, &slot, 0);
}

static void discardPendingNetworkLogs()
{
    if (g_netActiveSlot >= 0)
    {
        releaseNetworkLogSlot(static_cast<uint8_t>(g_netActiveSlot));
        g_netActiveSlot = -1;
        g_netActiveOffset = 0;
    }
    uint8_t slot;
    while (xQueueReceive(g_netPending, &slot, 0) == pdTRUE)
        releaseNetworkLogSlot(slot);
}

void debugConsole_serviceNetwork()
{
    if (!g_netPending) return;

    if (WiFi.status() != WL_CONNECTED)
    {
        if (g_netLogClient) g_netLogClient.stop();
        if (g_netServerStarted)
        {
            g_netLogServer.end();
            g_netServerStarted = false;
        }
        discardPendingNetworkLogs();
        return;
    }

    if (!g_netServerStarted)
    {
        g_netLogServer.begin();
        g_netServerStarted = static_cast<bool>(g_netLogServer);
        if (!g_netServerStarted) return;
        LOG_INFO(TAG_LOGGER, "Network log TCP port %u ready", NET_LOG_PORT);
    }
    if (g_netLogServer.hasClient())
    {
        WiFiClient incoming = g_netLogServer.accept();
        if (incoming)
        {
            g_netLogClient.stop();
            g_netLogClient = incoming;
            discardPendingNetworkLogs();
            LOG_INFO(TAG_LOGGER, "Network log monitor connected");
        }
    }
    if (!g_netLogClient.connected())
    {
        discardPendingNetworkLogs();
        return;
    }

    // A nonblocking send also prevents a stalled monitor from delaying OTA.
    for (int sentLines = 0; sentLines < 4; ++sentLines)
    {
        if (g_netActiveSlot < 0)
        {
            uint8_t slot;
            if (xQueueReceive(g_netPending, &slot, 0) != pdTRUE) break;
            g_netActiveSlot = slot;
            g_netActiveOffset = 0;
        }
        const uint8_t slot = static_cast<uint8_t>(g_netActiveSlot);
        const size_t remaining = g_netLineLengths[slot] - g_netActiveOffset;
        const int written = send(g_netLogClient.fd(), g_netLines[slot] + g_netActiveOffset,
                                 remaining, MSG_DONTWAIT);
        if (written > 0)
        {
            g_netActiveOffset += static_cast<size_t>(written);
            if (g_netActiveOffset < g_netLineLengths[slot]) break;
            releaseNetworkLogSlot(slot);
            g_netActiveSlot = -1;
            continue;
        }
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        g_netLogClient.stop();
        discardPendingNetworkLogs();
        break;
    }
}

// ────────────────────────────────────────────────────
// Hash-Eintrag (Vorschlag D: open-addressing statt linearem Scan)
// ────────────────────────────────────────────────────
struct SmartLogEntry
{
    const char  *file;
    int          line;
    uint32_t     msgHash;
    int          count;
    uint32_t     lastLogTime;
};

static SmartLogEntry hashMap[HASH_MAP_SIZE]; // 64 * 24 B = ~1.5 KB

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
