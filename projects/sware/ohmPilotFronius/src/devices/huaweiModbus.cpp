// =============================================================================
// huaweiModbus.cpp — Huawei SUN2000 Speicher via Modbus/TCP
//
// Register-Karten stehen in huaweiModbus.h.
// Die Adressen basieren auf Huawei Modbus Protocol Specification:
//   "Huawei-SUN2000-Modbus-Communication-Protocol-01V01-EN"
// =============================================================================

#include "devices/huaweiModbus.h"
#include "debugConsole.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ------------------------------------------------------------------
// Modbus TCP helper — raw socket (kein externer Modbus-Stack nötig)
// ------------------------------------------------------------------

// ── Modbus TCP-Header ───────────────────────────────────────────
// Transaction ID (2), Protocol ID (2), Length (2), Unit ID (1),
// Function Code (1), Register Address (2), Quantity (2)
// Response: ... + Byte Count (1) + Data (N)

struct __attribute__((packed)) ModbusTcpRequest
{
    uint16_t transId;
    uint16_t protoId;
    uint16_t length;
    uint8_t  unitId;
    uint8_t  funcCode;
    uint16_t startAddr;
    uint16_t quantity;
};

struct __attribute__((packed)) ModbusTcpResponse
{
    uint16_t transId;
    uint16_t protoId;
    uint16_t length;
    uint8_t  unitId;
    uint8_t  funcCode;
    uint8_t  byteCount;
};

static const uint16_t MB_TRANS_ID   = 0x1234;
static const uint16_t MB_PROTO_ID   = 0;
static const uint32_t MB_READ_TIMEOUT_MS = 2000;

// ------------------------------------------------------------------
// readU16 — Modbus Read Holding Register (FC 0x03), 16-Bit
// ------------------------------------------------------------------

bool HuaweiModbus::readU16(uint16_t registerAddr, uint16_t &data)
{
    if (!m_connected || !m_client.connected())
    {
        if (!connect())
        {
            LOG_ERROR(TAG_MODBUS, "Huawei: reconnect failed");
            return false;
        }
    }

    ModbusTcpRequest req;
    req.transId   = MB_TRANS_ID;
    req.protoId   = MB_PROTO_ID;
    req.quantity  = 1;
    req.length    = 6;       // Unit(1) + FC(1) + Addr(2) + Qty(2)
    req.unitId    = HUAWAI_REG_DEVICE_ID;
    req.funcCode  = 0x03;    // Read Holding Register
    req.startAddr = registerAddr;

    m_client.write((uint8_t*)&req, sizeof(req));

    // ── Antwort warten ─────────────────────────────────────────────
    uint32_t waitStart = millis();
    while (m_client.available() == 0)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (millis() - waitStart > MB_READ_TIMEOUT_MS)
        {
            LOG_ERROR(TAG_MODBUS, "Huawei: Modbus read timeout (%d)", registerAddr);
            return false;
        }
    }

    // ── Header parsen ──────────────────────────────────────────────
    ModbusTcpResponse hdr;
    if (m_client.read((uint8_t*)&hdr, sizeof(hdr)) < (int)sizeof(hdr))
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: incomplete Modbus response header");
        return false;
    }

    if (hdr.funcCode & 0x80)
    {
        uint8_t errCode;
        m_client.read(&errCode, 1);
        LOG_ERROR(TAG_MODBUS, "Huawei: Modbus error FC=0x%02x err=%d",
                  hdr.funcCode, errCode);
        return false;
    }

    // ── 16-Bit-Daten lesen ─────────────────────────────────────────
    if (m_client.read((uint8_t*)&data, 2) < 2)
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: incomplete 16-bit data");
        return false;
    }

    return true;
}

// ------------------------------------------------------------------
// readFloat32 — Modbus Read Holding Register, 32-Bit Float (2 Registers)
//              Byte-Reihenfolge: Big-Endian (A B C D)
// ------------------------------------------------------------------

bool HuaweiModbus::readFloat32(uint16_t registerAddr, float &result)
{
    uint16_t raw[2];
    if (!readU16(registerAddr, raw[0]))
        return false;
    if (!readU16(registerAddr + 1, raw[1]))
        return false;

    uint32_t combined = ((uint32_t)raw[0] << 16) | (uint32_t)raw[1];
    memcpy(&result, &combined, 4);

    return true;
}

// ------------------------------------------------------------------
// readFloat — Convenience: liest Register und wendet Skalierung an
// ------------------------------------------------------------------

float HuaweiModbus::readFloatRaw(int16_t raw, float scale)
{
    return (float)raw * scale;
}

float HuaweiModbus::readFloat(uint16_t registerAddr)
{
    float val = 0.0f;
    if (!readFloat32(registerAddr, val))
    {
        static int lastWarn = 0;
        if (millis() - lastWarn > 30000)
        {
            LOG_ERROR(TAG_MODBUS, "Huawei: fallback 0.0 for reg 0x%04x", registerAddr);
            lastWarn = millis();
        }
    }
    return val;
}

// ------------------------------------------------------------------
// connect / disconnect
// ------------------------------------------------------------------

bool HuaweiModbus::connect()
{
    LOG_INFO(TAG_MODBUS, "Huawei: Modbus/TCP connect → %s:502",
             m_ip.toString().c_str());

    if (m_client.connected())
        return true;

    if (!m_client.connect(m_ip, 502))
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: connection refused");
        m_connected = false;
        return false;
    }

    // Kleine Pause nach Verbindung
    vTaskDelay(pdMS_TO_TICKS(500));
    m_connected = true;
    LOG_INFO(TAG_MODBUS, "Huawei: Modbus/TCP connected");
    return true;
}

bool HuaweiModbus::disconnect()
{
    m_client.stop();
    m_connected = false;
    return true;
}
 
// ------------------------------------------------------------------
// init — Modbus/TCP-Verbindung herstellen
// ------------------------------------------------------------------

bool HuaweiModbus::init(Setup &setupData, WEBSOCK_DATA &webSockData)
{
    (void)webSockData;

    // ── Ziel-IP aus Setup-Struct ────────────────────────────────────
    // Reuses setupData.inverter field (IP-Adresse der Inverter/Speicher)
    if (setupData.inverter[0] == '\0')
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: no target IP configured in setupData.inverter");
        return false;
    }

    if (!m_ip.fromString(setupData.inverter))
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: cannot parse IP \"%s\"", setupData.inverter);
        return false;
    }

    if (!connect())
        return false;

    // ── Test-Read: Device Info (Register 0x0000) ──────────────────
    uint16_t deviceStatus = 0;
    if (readU16(0x0000, deviceStatus))
    {
        LOG_INFO(TAG_MODBUS, "Huawei: device status = 0x%04x", deviceStatus);
    }

    webSockData.setupData.akku = 1; // Huawei SUN2000 hat Batterie
    return true;
}

// ------------------------------------------------------------------
// read — alle Energie-Register lesen → energyState
// ------------------------------------------------------------------

bool HuaweiModbus::read(WEBSOCK_DATA &webSockData)
{
    auto &dst = webSockData.energyState;
    bool success = true;

    // ── PV-Leistung (float) ────────────────────────────────────────
    dst.p_pv = readFloat(HUAWAI_REG_PV_POWER);

    // ── Netzleistung (float) ──────────────────────────────────────
    dst.p_grid = readFloat(HUAWAI_REG_GRID_POWER);

    // ── Last (float) ──────────────────────────────────────────────
    dst.p_load = readFloat(HUAWAI_REG_LOAD_POWER);

    // ── Batterie-Leistung (float) ─────────────────────────────────
    dst.p_akku = readFloat(HUAWAI_REG_BATT_POWER);

    // ── Batterie SoC (0.1 % → integer / 10) ───────────────────────
    uint16_t rawSoc = 0;
    if (readU16(HUAWAI_REG_BATT_SOC, rawSoc))
    {
        dst.soc = (double)rawSoc / 10.0;
    }
    else
    {
        dst.soc = 0.0;
        success = false;
        LOG_ERROR(TAG_MODBUS, "Huawei: SoC read failed, using 0");
    }

    // ── Logging ───────────────────────────────────────────────────
    LOG_INFO(TAG_APP_SERVICES,
             "energy (huawei): pv=%.0fW grid=%.0fW load=%.0fW batt=%.0fW soc=%.1f%%",
             dst.p_pv, dst.p_grid, dst.p_load, dst.p_akku, dst.soc);

    // Kleiner Delay zwischen zyklischen Reads
    vTaskDelay(pdMS_TO_TICKS(100));

    return success;
}
