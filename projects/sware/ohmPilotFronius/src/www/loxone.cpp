#ifdef LOXONE

#include <ArduinoJson.h>
#include <JSONVar.h>
#include <JSON.h>
#include <ModbusIP_ESP8266.h>

#include "loxone.h"
#include "debugConsole.h"
#include "utils.h"
#include "temp.h"

#define PATH_NAME_LOXONE "/dev/sps/io/boilerPower_watt/" // LOXONE, virtueller eingang
#define LOXONE_USER "api_user"
#define LOXONE_PASSWD "derMannOhneEigenschaften"

static IPAddress remote;

/* ****************** PROTOTYPES **********************************************/

// static bool readJsonResponse(HTTP_REST_TARGET_t *target, WEBSOCK_DATA &webSockData);

static const size_t BUF_SIZE = 7 + strlen(LOXONE) + strlen(PATH_NAME_LOXONE) + 5 + 1;
static char base_url[BUF_SIZE];
static size_t base_len = 0;
static char urlBuf[BUF_SIZE];

// initialize the rest API targets
bool loxone_init(WEBSOCK_DATA &webSockData)
{
    strcpy(base_url, "http://");
    strcat(base_url, LOXONE);
    strcat(base_url, PATH_NAME_LOXONE);

    // Länge der Basis-URL für später merken
    base_len = strlen(base_url);
    size_t base_len = strlen(base_url);
    webSockData.states.loxone = false;
    itoa(0, base_url + base_len, 10);
    bool success = util_SendLoxone(base_url, LOXONE_USER, LOXONE_PASSWD);
    webSockData.states.loxone = success;
    return success;
}

static float heizleistungEinePhase = -1;
bool loxone_sendWattUsed(WEBSOCK_DATA &webSockData)
{

    if (heizleistungEinePhase == -1)
        heizleistungEinePhase = webSockData.setupData.heizstab_leistung_in_watt / 3; // heizleistung pro heizkreis
    int currentWatt = (int)((float)webSockData.pidContainer.mAnalogOut / 100.0 * heizleistungEinePhase);

    currentWatt += webSockData.pidContainer.PID_PIN1 * heizleistungEinePhase + webSockData.pidContainer.PID_PIN2 * heizleistungEinePhase;

    LOG_DEBUG(TAG_LOXONE, "Current Watt: %d", currentWatt);
    /*  webSockData.pidContainer.mAnalogOut ;
     webSockData.pidContainer.PID_PIN1 ;
     webSockData.pidContainer.PID_PIN2; */

    itoa(0, base_url + base_len, 10);
    bool success = util_SendLoxone(base_url, LOXONE_USER, LOXONE_PASSWD);

    if (success != 200)
    {
        LOG_ERROR(TAG_LOXONE, "loxone_sendWattUsed:: ResponsCode != 200");
        return false;
    }

    // LOG_INFO(TAG_AMIS, "Einspeisung: %d, Bezug: %d", einspeisung, bezug);

    return true;
}

void loxone_prepare(WEBSOCK_DATA &webSockData, LOXONE_INFO &loxone)
{

    if (heizleistungEinePhase == -1)
        heizleistungEinePhase = webSockData.setupData.heizstab_leistung_in_watt / 3; // heizleistung pro heizkreis
    loxone.usedWatt = (int)((float)webSockData.pidContainer.mAnalogOut / 100.0 * heizleistungEinePhase);

    loxone.usedWatt += webSockData.pidContainer.PID_PIN1 * heizleistungEinePhase + webSockData.pidContainer.PID_PIN2 * heizleistungEinePhase;

    LOG_DEBUG(TAG_LOXONE, "Current Watt: %d", loxone.usedWatt);
    /*  webSockData.pidContainer.mAnalogOut ;
     webSockData.pidContainer.PID_PIN1 ;
     webSockData.pidContainer.PID_PIN2; */
    loxone.error = 0;
    loxone.boilerTemp = (webSockData.temperature.sensor1 + webSockData.temperature.sensor2) / 2;
     LOG_DEBUG(TAG_LOXONE, "Current temp: %d", loxone.boilerTemp);
}

#endif