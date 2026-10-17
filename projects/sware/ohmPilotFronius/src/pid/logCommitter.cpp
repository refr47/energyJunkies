// =============================================================================
// logCommitter.cpp — LogCommitter Implementation
//
// Pure utility class for LogEntry filling and RingBuffer writes.
// No PinManager state required — all data passed as parameters.
// =============================================================================

#include "logCommitter.h"
#include "utils.h" // utils_logWrite

// ──────────────────────────────────────────────────────────────────────────
// fillEntry — populate LogEntry fields
// ──────────────────────────────────────────────────────────────────────────
void LogCommitter::fillEntry(LogEntry &le, unsigned long now,
                            uint8_t state, int power, uint8_t pwm, int temp)
{
    le.ts = now;
    le.state = state;
    le.power = power;
    le.pwm = pwm;
    le.temp = temp;
}

// ──────────────────────────────────────────────────────────────────────────
// writeToLog — push entry into RingBuffer
// ──────────────────────────────────────────────────────────────────────────
void LogCommitter::writeToLog(const LogEntry &le, WEBSOCK_DATA &webSockData)
{
    utils_logWrite(webSockData.logBuffer, le);
}

// ──────────────────────────────────────────────────────────────────────────
// commit — fill + write in one convenience call
// ──────────────────────────────────────────────────────────────────────────
void LogCommitter::commit(unsigned long now, uint8_t state, int power,
                         uint8_t pwm, int temp, WEBSOCK_DATA &webSockData)
{
    LogEntry le;
    fillEntry(le, now, state, power, pwm, temp);
    writeToLog(le, webSockData);
}
