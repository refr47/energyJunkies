// =============================================================================
// huaweiModbus.cpp — Huawei SUN2000-10KTL-M1 via Modbus/TCP
//
// Register-Map: Huawei-SUN2000-Modbus-Communication-Protocol-01V01-EN
// =============================================================================

#include "devices/huaweiModbus.h"
#include "debugConsole.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ------------------------------------------------------------------
// Modbus-TCP-Header (MBAP)
// ------------------------------------------------------------------

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

static const uint16_t MB_PROTO_ID        = 0;
static const uint32_t MB_READ_TIMEOUT_MS = 3000;
static const uint32_t MB_POLL_INTERVAL_MS = 50;

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
// readBatch — atomarer Modbus FC03-Request (N Register, max 125)
// ------------------------------------------------------------------

bool HuaweiModbus::readBatch(uint16_t startAddr, uint16_t quantity, uint16_t *buf)
{
    if (!m_connected || !m_client.connected())
    {
        if (!connect())
        {
            LOG_ERROR(TAG_MODBUS, "Huawei: reconnect failed");
            m_connected = false;
            return false;
        }
    }

    ModbusTcpRequest req;
    req.transId   = m_transId++;
    req.protoId   = MB_PROTO_ID;
    req.quantity  = quantity;
    req.length    = 6;
    req.unitId    = HUAWAI_REG_DEVICE_ID;
    req.funcCode  = 0x03;
    req.startAddr = startAddr;

    m_client.write((uint8_t*)&req, sizeof(req));

    // ── Antwort warten (kompletter Header) ──────────────────────────
    ModbusTcpResponse hdr;
    uint32_t t0 = millis();
    while (m_client.available() < (int)sizeof(hdr))
    {
        vTaskDelay(pdMS_TO_TICKS(MB_POLL_INTERVAL_MS));
        if (millis() - t0 > MB_READ_TIMEOUT_MS)
        {
            LOG_ERROR(TAG_MODBUS, "Huawei: timeout header (0x%04x)", startAddr);
            m_connected = false;
            return false;
        }
    }

    if (m_client.read((uint8_t*)&hdr, sizeof(hdr)) < (int)sizeof(hdr))
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: short header");
        m_connected = false;
        return false;
    }

    if (hdr.funcCode & 0x80)
    {
        uint8_t errCode = 0;
        if (m_client.available() > 0)
            m_client.read(&errCode, 1);
        LOG_ERROR(TAG_MODBUS, "Huawei: error FC=0x%02x err=%d",
                  hdr.funcCode, errCode);
        m_connected = false;
        return false;
    }

    // ── Payload lesen ───────────────────────────────────────────────
    size_t expectedBytes = hdr.byteCount;
    uint8_t tmp[254];

    uint32_t t1 = millis();
    while (m_client.available() < expectedBytes)
    {
        vTaskDelay(pdMS_TO_TICKS(MB_POLL_INTERVAL_MS));
        if (millis() - t1 > MB_READ_TIMEOUT_MS)
        {
            LOG_ERROR(TAG_MODBUS, "Huawei: timeout payload (%u/%u)",
                      m_client.available(), expectedBytes);
            m_connected = false;
            return false;
        }
    }

    if (m_client.read(tmp, expectedBytes) < (int)expectedBytes)
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: short payload");
        m_connected = false;
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        buf[i] = ((uint16_t)tmp[2 * i] << 8) | (uint16_t)tmp[2 * i + 1];
    }

    return true;
}

// ------------------------------------------------------------------
// connect / disconnect
// ------------------------------------------------------------------

bool HuaweiModbus::connect()
{
    LOG_INFO(TAG_MODBUS, "Huawei: connect → %s:502",
             m_ip.toString().c_str());

    if (m_client.connected())
        return true;

    if (!m_client.connect(m_ip, 502))
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: connection refused");
        m_connected = false;
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    m_connected = true;
    LOG_INFO(TAG_MODBUS, "Huawei: connected");
    return true;
}

bool HuaweiModbus::disconnect()
{
    m_client.stop();
    m_connected = false;
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

    if (!m_ip.fromString(setupData.inverter))
    {
        LOG_ERROR(TAG_MODBUS, "Huawei: bad IP \"%s\"", setupData.inverter);
        return false;
    }

    if (!connect())
        return false;

    // Test-Read
    {
        uint16_t testBuf[2];
        if (readBatch(0x7D80, 2, testBuf))
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
    if (!m_connected || !m_client.connected())
    {
        if (!connect())
        {
            LOG_ERROR(TAG_MODBUS, "Huawei: not connected, read skipped");
            m_connected = false;
            return false;
        }
    }

    float pvRaw   = 0.0f;
    float gridRaw = 0.0f;
    float battRaw = 0.0f;
    double socVal = 0.0;

    // Batch 1: PV Power (0x7D80)
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
