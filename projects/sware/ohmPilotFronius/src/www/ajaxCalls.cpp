#include "ajaxCalls.h"
#include "eprom.h"
#include "utils.h"

/*
    *******************  PROTOTYPES

*/
static void returnFromStoreSetup(bool inputCorrect, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, AsyncWebServerRequest *request);
static void dbg(char *key, const char *val);
static void handleGetSetup(AsyncWebServerRequest *request);

void ajaxCalls_handleGetSetup(AsyncWebServerRequest *request)
{
    Setup setup;
    eprom_getSetup(setup);
    StaticJsonDocument<JSON_OBJECT_SETUP_LEN> data;
    char buff[50];

    data[WLAN_ESSID] = setup.ssid;
    data[WLAN_PASSWD] = setup.passwd;
    data[IP_INVERTER] = setup.ipInverterAsString;

    sprintf(buff, "%d", setup.regelbereichHysterese);
    data[HYSTERESE] = buff;
    sprintf(buff, "%d", setup.pid_targetPowerInWatt);
    data[EINSPEISUNG_MUSS] = buff;
    sprintf(buff, "%d", setup.pid_min_time_before_switch_off_channel_inMS);
    data[MINDEST_LAUFZEIT_DIGITALER_OUT] = buff;
    sprintf(buff, "%d", setup.pid_min_time_without_contoller_inMS);
    data[MINDEST_LAUFZEIT_PORT_ON] = buff;
    sprintf(buff, "%d", setup.pid_min_time_for_dig_output_inMS);
    data[MINDEST_LAUFZEIT_REGLER_KONSTANT] = buff;
    data[EXTERNER_SPEICHER] = setup.externerSpeicher ? "j'" : "n";
    sprintf(buff, "%c", setup.externerSpeicherPriori);
    data[EXTERNER_SPEICHER_PRIORI] = buff;
    sprintf(buff, "%d", setup.ausschaltTempInGradCel);
    data[AUSSCHALT_TEMP] = buff;
    sprintf(buff, "%f.2", setup.pid_p);
    data[PID_P] = buff;
    sprintf(buff, "%f.2", setup.pid_i);
    data[PID_I] = buff;
    sprintf(buff, "%f.2", setup.pid_d);
    data[PID_D] = buff;
    

        return returnFromStoreSetup(true, data, request);
}

void ajaxCalls_handleStoreSetup(AsyncWebServerRequest *request, JsonVariant &json)
{
    const JsonObject &jsonObj = json.as<JsonObject>();
    StaticJsonDocument<JSON_OBJECT_SETUP_LEN> data;

    for (JsonPair keyValue : jsonObj)
    {
        Serial.print(keyValue.key().c_str());
    }
    char *cP = NULL;
    Setup setup; // eprom write
    const char *argument = jsonObj[WLAN_ESSID];
    int result = 0;
    float resultF = 0.0;
    bool errorH = false;
    DBGln("STart parsing json object");
    dbg(WLAN_ESSID, argument);
    errorH = util_isFieldFilled(WLAN_ESSID, argument, data);
    if (errorH)
        strcpy(setup.ssid, jsonObj[WLAN_ESSID]);
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[WLAN_PASSWD];
    dbg(WLAN_PASSWD, argument);
    errorH = util_isFieldFilled(WLAN_PASSWD, argument, data);
    if (errorH)
        strcpy(setup.passwd, jsonObj[WLAN_PASSWD]);
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[IP_INVERTER];
    dbg(IP_INVERTER, argument);
    errorH = util_isFieldFilled(IP_INVERTER, argument, data);
    if (errorH)
    {
        bool result = true;
        String ipInv = argument;
        if (!result)
        {
            DBG("handleStoreSetup IP translate did not succeed for IP: ");
            DBGln(argument);
            data["error"] = "IP-Transformation war nicht erfolgreich!";
            return returnFromStoreSetup(false, data, request);
        }
        else
            setup.ipInverter = ipv4_string_to_int(ipInv, &result);
    }
    else
        return returnFromStoreSetup(errorH, data, request);

    argument = jsonObj[HYSTERESE];
    dbg(HYSTERESE, argument);
    errorH = util_checkParamInt(HYSTERESE, argument, data, &result);
    if (errorH)
        setup.regelbereichHysterese = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[EINSPEISUNG_MUSS];
    dbg(EINSPEISUNG_MUSS, argument);
    errorH = util_checkParamInt(EINSPEISUNG_MUSS, argument, data, &result);
    if (errorH)
        setup.pid_targetPowerInWatt = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[MINDEST_LAUFZEIT_DIGITALER_OUT];
    dbg(MINDEST_LAUFZEIT_DIGITALER_OUT, argument);

    errorH = util_checkParamInt(MINDEST_LAUFZEIT_DIGITALER_OUT, argument, data, &result);
    if (errorH)
        setup.pid_min_time_before_switch_off_channel_inMS = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[MINDEST_LAUFZEIT_PORT_ON];
    dbg(MINDEST_LAUFZEIT_PORT_ON, argument);
    errorH = util_checkParamInt(MINDEST_LAUFZEIT_PORT_ON, argument, data, &result);
    if (errorH)
        setup.pid_min_time_for_dig_output_inMS = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[MINDEST_LAUFZEIT_REGLER_KONSTANT];
    dbg(MINDEST_LAUFZEIT_REGLER_KONSTANT, argument);
    errorH = util_checkParamInt(MINDEST_LAUFZEIT_REGLER_KONSTANT, argument, data, &result);
    if (errorH)
        setup.pid_min_time_without_contoller_inMS = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[EXTERNER_SPEICHER];
    dbg(EXTERNER_SPEICHER, argument);
    if (util_isFieldFilled(EXTERNER_SPEICHER, argument, data))
    {
        setup.externerSpeicher = *argument == 'j' ? true : false;
    }
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[EXTERNER_SPEICHER_PRIORI];
    dbg(EXTERNER_SPEICHER_PRIORI, argument);
    if (util_isFieldFilled(EXTERNER_SPEICHER_PRIORI, argument, data))
    {
        setup.externerSpeicherPriori = *argument; // j1|2|3
    }
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[AUSSCHALT_TEMP];
    dbg(AUSSCHALT_TEMP, argument);
    errorH = util_checkParamInt(AUSSCHALT_TEMP, argument, data, &result);
    if (errorH)
        setup.ausschaltTempInGradCel = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[PID_P];
    dbg(PID_P, argument);
    errorH = util_checkParamFloat(PID_P, argument, data, &resultF);
    if (errorH)
        setup.pid_p = resultF;
    argument = jsonObj[PID_I];
    dbg(PID_I, argument);

    errorH = util_checkParamFloat(PID_I, argument, data, &resultF);
    if (errorH)
        setup.pid_i = resultF;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[PID_D];
    dbg(PID_D, argument);
    errorH = util_checkParamFloat(PID_D, argument, data, &resultF);
    if (errorH)
        setup.pid_d = resultF;
    else
        return returnFromStoreSetup(errorH, data, request);

    eprom_storeSetup(setup);
    eprom_test_read_Eprom();
    return returnFromStoreSetup(errorH, data, request);
    // esp_restart();
}
/* private functions */

static void returnFromStoreSetup(bool inputCorrect, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, AsyncWebServerRequest *request)
{
    String response;
    if (inputCorrect)
    {
        data["done"] = 1;
        data["error"] = "";
        DBGln("returnFromStoreSetup - no errors ");
        // eprom_storeSetup(setup);
    }
    else
    {
        data["done"] = 0;
        DBGln("returnFromStoreSetup -  errors ");
    }

    DBGln(" vor serialisierung : ");
    serializeJson(data, response);
    DBG("storeSetUp - return: ");
    DBGln(response);

    request->send(200, "application/json", response);
}

static void dbg(char *key, const char *val)
{
    DBG("ARG: ");
    DBG(key);
    DBG(" VAL: ");
    DBGln(val);
}
