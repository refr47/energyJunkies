#pragma once
#include <Arduino.h>
#include "defines.h"

// =============================================================================
// LogCommitter — LogEntry creation, filling \& log-buffer write
//
// Encapsulates the 9× repeated boilerplate in PinManager::update():
//   1. create LogEntry struct
//   2. fill fields (ts, state, power, pwm, temp)
//   3. write into RingBuffer via utils_logWrite()
//
// Pure utilities — no PinManager state required.
// pidLockWrite() (WebSocket push) stays in PinManager because it reads
// PinManager's own members (phaseCtrl, weather, tinyNN).
// =============================================================================
class LogCommitter
{
public:
    // ── Fill LogEntry fields ────────────────────────────────────────────
    static void fillEntry(LogEntry &le, unsigned long now,
                         uint8_t state, int power, uint8_t pwm, int temp);

    // ── Write entry into the RingBuffer log ─────────────────────────────
    // Thin wrapper around utils_logWrite(webSockData.logBuffer, le)
    static void writeToLog(const LogEntry &le, WEBSOCK_DATA &webSockData);

    // ── Convenience: fill + write in one call ──────────────────────────
    static void commit(unsigned long now, uint8_t state, int power,
                      uint8_t pwm, int temp, WEBSOCK_DATA &webSockData);
};
