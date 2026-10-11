/*
 * ──────────────────────────────────────────────────────
 *  Modbus Reader  –  Fronius Inverter (Modbus/TCP)
 *
 *  Liest nur die Inverter-Summary-Blöcke:
 *    AC current power (kW), AC total energy, DC current power
 *
 *  Alle anderen Blöcke (SmartMeter, Akku-State, Akku-Strg)
 *  kommen über Fronius Solar API / AMIS Reader – nicht Modbus.
 * ──────────────────────────────────────────────────────
 */

#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <ModbusIP_ESP8266.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "defines.h"
#include "modbusReader.h"
#include "modbusRegister.h"

/* ─── Register-Layout (Fronius) ─────────────────── */

#define MB_DEVICE_ID         1          // Inverter
#define MB_REG_START         40083      // holding register base
#define MB_REG_COUNT         19         // 40083 .. 40101
#define MB_WAIT_MS           500        // Wartezeit nach Read

/* Skalierung: 3 Werte aus 19 Registern
 *   idx 0: acCurrentPower  → reg 0/1 (40083/84), scale reg 1
 *   idx 1: acTotalEnergy   → reg 10/11 (40093/95), scale reg 1 (2 regs)
 *   idx 2: dcCurrentPower  → reg 17/18 (40100/101), scale reg 1
 */
static SCALE_INDEX_t scaleInverterSum[INVERTER_SUM_VALUE_LEN] = {
    {0, 40083 - MB_REG_START, 40084 - MB_REG_START, 1},
    {1, 40093 - MB_REG_START, 40095 - MB_REG_START, 2},
    {2, 40100 - MB_REG_START, 40101 - MB_REG_START, 1}
};

/* ─── Modbus-Instanz (statisch) ──────────────────── */

static IPAddress remote;
static ModbusIP mb;

/* ─── private helpers ────────────────────────────── */

static bool mb_connect(Setup &setup)
{
    if (!remote.fromString(setup.inverter))
    {
        LOG_ERROR(TAG_MODBUS, "mb_connect: cannot parse IP \"%s\"", setup.inverter);
        return false;
    }

    if (!mb.isConnected(remote))
    {
        bool ok = mb.connect(remote);
        vTaskDelay(pdMS_TO_TICKS(1000));
        mb.client();

        if (!ok)
            LOG_ERROR(TAG_MODBUS, "mb_connect: connection failed (errno %d)", errno);

        return ok;
    }

    return true;
}

/* ─── public API ─────────────────────────────────── */

bool mb_init(Setup &setup)
{
    LOG_INFO(TAG_MODBUS, "mb_init: addr=%s", setup.inverter);
    return mb_connect(setup);
}

bool mb_readInverter(Setup &setup, MB_CONTAINER &container)
{
    // ── Verbindung sicherstellen ──────────────────
    if (!mb.isConnected(remote) && !mb_connect(setup))
        return false;

    // ── Holding-Registers lesen ───────────────────
    static INVERTER_SUM_VALUE_t buf;
    static int16_t regs[MB_REG_COUNT];
    uint16_t transId = mb.readHreg(remote, MB_REG_START,
                                  (uint16_t *)regs, MB_REG_COUNT,
                                  NULL, MB_DEVICE_ID);

    if (transId == 0)
    {
        LOG_ERROR(TAG_MODBUS, "mb_readInverter: register read failed");
        vTaskDelay(pdMS_TO_TICKS(5000));
        return false;
    }

    // Asyncher Modbus-Read → auf Antwort warten
    mb.task();
    vTaskDelay(pdMS_TO_TICKS(MB_WAIT_MS));

    // ── Skalieren ─────────────────────────────────
    scaleValues(buf.value, regs, scaleInverterSum, INVERTER_SUM_VALUE_LEN);

#ifdef MODBUS_VERBOSE
    LOG_DEBUG(TAG_MODBUS, "Inverter: acPwr=%.3lf acEnergy=%.3lf dcPwr=%.3lf",
              buf.data.acCurrentPower,
              buf.data.acTotalEnergy,
              buf.data.dcCurrentPower);
#endif

    // ── In Container kopieren ────────────────────
    memcpy(&container.inverterSumValues.data, &buf.data, sizeof(buf.data));

    return true;
}
