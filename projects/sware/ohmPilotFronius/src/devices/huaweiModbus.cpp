// =============================================================================
// huaweiModbus.cpp — Huawei SUN2000-10KTL-M1 via Modbus/TCP
//
// Register-Map: Huawei-SUN2000-Modbus-Communication-Protocol-01V01-EN
// Uses ModbusIP_ESP8266 library — konsistent zum Fronius Modbus-Reader.
// =============================================================================

#include "devices/huaweiModbus.h"
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
// parseFloat32 — Big-Endian ABCD (Reg[0]=Hi, Reg[1]=Lo)
// ------------------------------------------------------------------

float HuaweiModbus::parseFloat32(uint16_t hi, uint16_t lo)
{
    uint32_t combined = ((uint32_t)hi << 16) | (uint32_t)lo;
    float result;
    memcpy(&result, &combined, 4);
    return result;
}

// ------------------------------------------------------------------
// connect — ModbusIP-Verbindung zum Huawei-Inverter
// ------------------------------------------------------------------

bool HuaweiModbus::connect(Setup &setupData)
{
    if (!m_ip.fromString(setupData.inverter))
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: bad IP \"%s\"", setupData.inverter);
        return false;
    }

    if (mbHuawei.isConnected(m_ip))
        return true;

    LOG_INFO(TAG_MODBUS, "Huawei: connect → %s:502", m_ip.toString().c_str());

    bool ok = mbHuawei.connect(m_ip);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (!ok)
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: connection refused");
        m_connected = false;
        return false;
    }

    m_connected = true;
    LOG_INFO(TAG_MODBUS, "Huawei: connected");
    return true;
}

// ------------------------------------------------------------------
// readBatch — Modbus FC03 via ModbusIP
// ------------------------------------------------------------------

bool HuaweiModbus::readBatch(uint16_t startAddr, uint16_t quantity, uint16_t *buf)
{
    if (!m_connected || !mbHuawei.isConnected(m_ip))
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: not connected, reconnect skipped");
        m_connected = false;
        return false;
    }

    // ModbusIP sendet asynchron, transId==0 bedeutet Fehler
    uint16_t transId = mbHuawei.readHreg(m_ip, startAddr, buf, quantity,
                                         NULL, HUAWAI_REG_DEVICE_ID);

    if (transId == 0)
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: readHreg failed at 0x%04x", startAddr);
        m_connected = false;
        return false;
    }

    // Auf Antwort warten
    mbHuawei.task();
    vTaskDelay(pdMS_TO_TICKS(100));

    return true;
}

// ------------------------------------------------------------------
// init
// ------------------------------------------------------------------

bool HuaweiModbus::init(Setup &setupData, WEBSOCK_DATA &webSockData)
{
    if (setupData.inverter[0] == '\0')
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: no target IP");
        return false;
    }

    if (!connect(setupData))
        return false;

    // Test-Read
    {
        uint16_t testBuf[2];
        if (readBatch(HUAWAI_REG_ACTIVE_POWER, 2, testBuf))
        {
            float pv = parseFloat32(testBuf[0], testBuf[1]);
            LOG_INFO(TAG_MODBUS, "Huawei: init OK, PV=%.1f W", pv);
        }
    }

    webSockData.setupData.akku = 1;
    return true;
}

// ------------------------------------------------------------------
// read - 3 Batches, sofort Konventionenumwandlung
//   All-or-nothing: bei fehlgeschlagenem Batch wird sofort abgebrochen,
//   energyState bleibt unveraendert (alte gueltige Werte).
//   m_connected wird bei jedem Fehler auf false gesetzt.
// ------------------------------------------------------------------

bool HuaweiModbus::read(WEBSOCK_DATA &webSockData)
{
    if (!m_connected || !mbHuawei.isConnected(m_ip))
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: not connected, read skipped");
        m_connected = false;
        return false;
    }

    float pvRaw   = 0.0f;
    float gridRaw = 0.0f;
    float battRaw = 0.0f;
    double socVal = 0.0;

    // Batch 1: PV Power (0x7D80 → 0x7D90 = ACTIVE_POWER)
    {
        uint16_t buf[2];
        if (!readBatch(HUAWAI_REG_ACTIVE_POWER, 2, buf))
        {
            m_connected = false;
            LOG_ERROR(TAG_MODBUS, "Huawei: PV batch failed - connection lost");
            return false;
        }
        pvRaw = parseFloat32(buf[0], buf[1]);
    }

    // Batch 2: Grid Power (0x9129)
    {
        uint16_t buf[2];
        if (!readBatch(HUAWAI_REG_GRID_POWER, 2, buf))
        {
            m_connected = false;
            LOG_ERROR(TAG_MODBUS, "Huawei: grid batch failed - connection lost");
            return false;
        }
        gridRaw = parseFloat32(buf[0], buf[1]);
    }

    // Batch 3: SoC (0x9390) + Batt Power (0x9392)
    {
        uint16_t buf[3];
        if (!readBatch(HUAWAI_REG_BATT_SOC, 3, buf))
        {
            m_connected = false;
            LOG_ERROR(TAG_MODBUS, "Huawei: batt batch failed - connection lost");
            return false;
        }
        socVal  = (double)buf[0];
        battRaw = parseFloat32(buf[1], buf[2]);
    }

    // Nur wenn ALLE 3 Batches OK -> energyState beschreiben
    // Huawei: grid +1=Bezug, -1=Einsp -> negate
    // Huawei: batt +1=Laden, -1=Entlad -> negate
    // P_LOAD = PV + Netz_Bezug + Akku_Entladen
    auto &dst = webSockData.energyState;
    dst.p_pv   = pvRaw;
    dst.p_grid = -gridRaw;
    dst.p_akku = -battRaw;
    dst.p_load = pvRaw - gridRaw;
    dst.soc    = socVal;

    LOG_INFO(TAG_APP_SERVICES,
             "energy (huawei): pv=%.0fW grid=%.0fW load=%.0fW akku=%.0fW soc=%.1f%%",
             dst.p_pv, dst.p_grid, dst.p_load, dst.p_akku, dst.soc);

    vTaskDelay(pdMS_TO_TICKS(100));
    return true;
}
