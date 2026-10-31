// =============================================================================
// huaweiModbus.cpp — Huawei SUN2000-10KTL-M1 via Modbus/TCP
//
// Register-Map: Huawei-SUN2000-Modbus-Communication-Protocol-01V01-EN
// Uses ModbusIP_ESP8266 library — konsistent zum Fronius Modbus-Reader.
// =============================================================================

#include "huaweiModbus.h"
#include "debugConsole.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <ModbusIP_ESP8266.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ------------------------------------------------------------------
// ModbusIP-Instanz (getrennt vom Fronius-Reader)
// ------------------------------------------------------------------

static ModbusIP mbHuawei;

// ------------------------------------------------------------------
// parseInt32 — Big-Endian ABCD (Reg[0]=Hi, Reg[1]=Lo)
// ------------------------------------------------------------------

int32_t HuaweiModbus::parseInt32(uint16_t hi, uint16_t lo)
{
    uint32_t combined = ((uint32_t)hi << 16) | (uint32_t)lo;
    int32_t result;
    memcpy(&result, &combined, sizeof(result));
    return result;
}

// ------------------------------------------------------------------
// connect — ModbusIP-Verbindung zum Huawei-Inverter
// ------------------------------------------------------------------

bool HuaweiModbus::connect(Setup& setupData)
{
    IPAddress target;
    if (!target.fromString(setupData.inverter))
    {
        m_connected = false;
        LOG_ERROR(TAG_HUAWEI, "Huawei: bad IP \"%s\"", setupData.inverter);
        return false;
    }

    if (m_ip != target)
        mbHuawei.disconnect(m_ip);
    m_ip = target;

    // Geschlossene Sockets vor dem naechsten Verbindungsversuch freigeben.
    mbHuawei.task();
    m_connected = WiFi.status() == WL_CONNECTED && mbHuawei.isConnected(m_ip);
    if (m_connected)
        return true;
    if (WiFi.status() != WL_CONNECTED)
        return false;

    LOG_INFO(TAG_HUAWEI, "Huawei: connect → %s:502", m_ip.toString().c_str());

    bool ok = mbHuawei.connect(m_ip);

    if (!ok)
    {
        LOG_ERROR(TAG_HUAWEI, "Huawei: TCP connect failed to %s:502", m_ip.toString().c_str());
        m_connected = false;
        return false;
    }

    m_connected = true;
    LOG_INFO(TAG_HUAWEI, "Huawei: connected");
    return true;
}

// ------------------------------------------------------------------
// readBatch — Modbus FC03 via ModbusIP
// ------------------------------------------------------------------

bool HuaweiModbus::readBatch(uint16_t startAddr, uint16_t quantity, uint16_t* buf)
{
    for (unsigned attempt = 0; attempt < 2; ++attempt)
    {
        const auto result = readBatchOnce(startAddr, quantity, buf);
        if (result == Modbus::EX_SUCCESS)
            return true;
        // Eine Server-Exception bedeutet nicht, dass TCP unterbrochen ist.
        if (attempt == 0 && m_connected &&
            (result == Modbus::EX_SLAVE_FAILURE || result == Modbus::EX_SLAVE_DEVICE_BUSY))
        {
            LOG_INFO(TAG_HUAWEI, "Huawei: retry register %u on existing connection", startAddr);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        return false;
    }
    return false;
}

uint8_t HuaweiModbus::readBatchOnce(uint16_t startAddr, uint16_t quantity, uint16_t* buf)
{
    if (!m_connected || !mbHuawei.isConnected(m_ip))
    {
        LOG_ERROR(TAG_HUAWEI, "Huawei: not connected, reconnect skipped");
        m_connected = false;
        return Modbus::EX_CONNECTION_LOST;
    }

    // Alle mbHuawei-Zugriffe muessen seriell in der aufrufenden Task erfolgen.
    // Callback und Puffer bleiben bis zum Transaktionsende gueltig.
    Modbus::ResultCode result = Modbus::EX_TIMEOUT;
    uint16_t transId = mbHuawei.readHreg(m_ip, startAddr, buf, quantity,
        [&result](Modbus::ResultCode event, uint16_t, void*) {
            result = event;
            return true;
        }, HUAWAI_REG_DEVICE_ID);

    if (transId == 0)
    {
        LOG_ERROR(TAG_HUAWEI, "Huawei: readHreg failed at 0x%04x", startAddr);
        m_connected = mbHuawei.isConnected(m_ip);
        return Modbus::EX_GENERAL_FAILURE;
    }

    // task() verarbeitet Antworten und den Bibliotheks-Timeout.
    // Kein vorzeitiger Abbruch mit noch offenen Pufferreferenzen.
    TickType_t pause = pdMS_TO_TICKS(5);
    if (pause == 0)
        pause = 1;
    while (mbHuawei.isTransaction(transId))
    {
        mbHuawei.task();
        if (mbHuawei.isTransaction(transId))
            vTaskDelay(pause);
    }
    if (result != Modbus::EX_SUCCESS)
    {
        LOG_ERROR(TAG_HUAWEI, "Huawei: FC03 unit=%u register=%u (0x%04x) count=%u failed: 0x%02x",
            HUAWAI_REG_DEVICE_ID, startAddr, startAddr, quantity, static_cast<unsigned>(result));
        m_connected = mbHuawei.isConnected(m_ip);
        // Lokale Transport-/Protokollfehler: Socket fuer den naechsten Zyklus erneuern.
        // Modbus-Exceptions (0x01..0x0B) lassen eine intakte Verbindung bestehen.
        if (static_cast<unsigned>(result) >= 0xE0)
        {
            mbHuawei.disconnect(m_ip);
            m_connected = false;
        }
        return static_cast<uint8_t>(result);
    }

    return Modbus::EX_SUCCESS;
}

// ------------------------------------------------------------------
// init
// ------------------------------------------------------------------

bool HuaweiModbus::init(Setup& setupData, WEBSOCK_DATA& webSockData)
{

    LOG_INFO(TAG_HUAWEI, "Huawei: init called from task=%s",
        pcTaskGetName(nullptr));
    if (setupData.inverter[0] == '\0')
    {
        LOG_ERROR(TAG_HUAWEI, "Huawei: no target IP");
        return false;
    }

    if (!connect(setupData))
        return false;

    // Test-Read
    {
        uint16_t testBuf[2];
        if (!readBatch(HUAWAI_REG_ACTIVE_POWER, 2, testBuf))
            return false;
        double activePower = parseInt32(testBuf[0], testBuf[1]);
        LOG_INFO(TAG_HUAWEI, "Huawei: init OK, AC=%.1f W", activePower);
    }

    webSockData.setupData.akku = 1;
    return true;
}

// ------------------------------------------------------------------
// read - 5 Abfragen, sofort Konventionenumwandlung
//   All-or-nothing: bei fehlgeschlagenem Batch wird sofort abgebrochen,
//   energyState bleibt unveraendert (alte gueltige Werte).
//   Modbus-Exceptions lassen eine intakte TCP-Verbindung bestehen.
// ------------------------------------------------------------------

bool HuaweiModbus::read(WEBSOCK_DATA& webSockData)
{
    // Ein Reconnect-Versuch je Messzyklus, keine endlose Retry-Schleife.
    if (!connect(webSockData.setupData))
    {
        LOG_ERROR(TAG_HUAWEI, "Huawei: reconnect failed, read skipped");
        m_connected = false;
        return false;
    }

    double pvRaw = 0.0;
    double activeRaw = 0.0;
    double gridRaw = 0.0;
    double battRaw = 0.0;
    double socVal = 0.0;

    // DC-Eingangsleistung getrennt von der AC-Ausgangsleistung lesen.
    {
        uint16_t buf[2];
        if (!readBatch(HUAWAI_REG_PV_POWER, 2, buf))
            return false;
        pvRaw = parseInt32(buf[0], buf[1]);
    }

    // AC-Ausgangsleistung fuer die Hausverbrauchsberechnung.
    {
        uint16_t buf[2];
        if (!readBatch(HUAWAI_REG_ACTIVE_POWER, 2, buf))
        {
            LOG_ERROR(TAG_HUAWEI, "Huawei: AC batch failed");
            return false;
        }
        activeRaw = parseInt32(buf[0], buf[1]);
    }

    // Batch 2: Grid Power (37113)
    {
        uint16_t buf[2];
        if (!readBatch(HUAWAI_REG_GRID_POWER, 2, buf))
        {
            LOG_ERROR(TAG_HUAWEI, "Huawei: grid batch failed");
            return false;
        }
        gridRaw = parseInt32(buf[0], buf[1]);
    }

    // SoC und Batterieleistung liegen laut Konstanten nicht aneinander.
    {
        uint16_t socBuf[1];
        uint16_t powerBuf[2];
        if (!readBatch(HUAWAI_REG_BATT_SOC, 1, socBuf) ||
            !readBatch(HUAWAI_REG_BATT_POWER, 2, powerBuf))
        {
            LOG_ERROR(TAG_HUAWEI, "Huawei: batt batch failed");
            return false;
        }
        socVal = socBuf[0] / 10.0;
        battRaw = parseInt32(powerBuf[0], powerBuf[1]);
    }

    // Nur wenn ALLE Abfragen OK -> energyState beschreiben
    // Huawei: grid positiv=Einspeisung, negativ=Bezug -> negate
    // Huawei: batt +1=Laden, -1=Entlad -> negate
    // AC-Ausgang enthaelt den Batterieanteil bereits; Hauslast ist negativ.
    ENERGY_STATE dst{};
    dst.p_pv = pvRaw;
    dst.p_grid = -gridRaw;
    dst.p_akku = -battRaw;
    dst.p_load = gridRaw - activeRaw;
    dst.soc = socVal;

    if (!publishSample(webSockData, dst)) {
        return false;
    }

    LOG_INFO(TAG_APP_SERVICES,
        "energy (huawei): pv=%.0fW grid=%.0fW load=%.0fW akku=%.0fW soc=%.1f%%",
        dst.p_pv, dst.p_grid, dst.p_load, dst.p_akku, dst.soc);

    return true;
}
