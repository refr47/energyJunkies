#ifdef LOXONE_API
#include "loxoneAsProvider.h"
#include "loxoneResponse.h"
#include "www/loxoneConfig.h"
#include "debugConsole.h"
#include <HTTPClient.h>

// Dieselben fünf Messgrößen wie FroniusHTTP::read().
// Die gleichnamigen Controls müssen im Miniserver verfügbar sein.
bool LoxoneAsProvider::readValue(const char* control, double& value)
{
    HTTPClient http;
    const String url = String("http://") + LOXONE + "/jdev/sps/io/" + control + "/state";
    http.setConnectTimeout(1500);
    http.setTimeout(1500);
    if (!http.begin(url)) return false;
    http.setAuthorization(LOXONE_USER, LOXONE_PASSWD);
    const int status = http.GET();
    bool ok = false;
    if (status == HTTP_CODE_OK) {
        const String payload = http.getString();
        ok = loxoneParseValue(payload.c_str(), value);
    }
    http.end();
    if (!ok) LOG_ERROR(TAG_LOXONE, "Read %s failed (HTTP %d)", control, status);
    return ok;
}

bool LoxoneAsProvider::init(Setup& setupData, WEBSOCK_DATA& webSockData)
{

    LOG_INFO(TAG_LOXONE, "Loxone: init called from task=%s",
        pcTaskGetName(nullptr));
    (void)setupData;
    m_hasBattery = false;
    // Erreichbarkeit allein reicht nicht: erst einen vollständigen Satz prüfen.
    const bool ok = read(webSockData);
    m_hasBattery = ok;
    return ok;
}

bool LoxoneAsProvider::read(WEBSOCK_DATA& webSockData)
{
    ENERGY_STATE sample{};
    if (!readValue("P_Grid", sample.p_grid) ||
        !readValue("P_Load", sample.p_load) ||
        !readValue("P_PV", sample.p_pv) ||
        !readValue("P_Akku", sample.p_akku) ||
        !readValue("SOC", sample.soc)) return false;
    if (sample.soc < 0 || sample.soc > 100) {
        LOG_ERROR(TAG_LOXONE, "SOC outside 0..100");
        return false;
    }
    // Vorzeichen entsprechen Fronius; kein Netzwerk-I/O unter DATA-Lock.
    return publishSample(webSockData, sample);
}
#endif
