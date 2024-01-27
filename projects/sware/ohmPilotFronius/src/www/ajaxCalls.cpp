#include "ajaxCalls.h"
#include "eprom.h"
#include "utils.h"

/*
    *******************  PROTOTYPES

*/
static void returnFromStoreSetup(bool inputCorrect, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, AsyncWebServerRequest *request);

static void handleGetSetup(AsyncWebServerRequest *request);

void ajaxCalls_handleGetSetup(AsyncWebServerRequest *request)
{
    Setup setup;
    eprom_getSetup(setup);
    StaticJsonDocument<JSON_OBJECT_SETUP_LEN> data;
    char buff[50];
    DBGf("ajaxCalls_handleGetSetup - begin");

    data[WLAN_ESSID] = setup.ssid;
    data[WLAN_PASSWD] = setup.passwd;
    data[IP_INVERTER] = setup.ipInverterAsString;

    sprintf(buff, "%d", setup.heizstab_leistung_in_watt);
    data[HEIZSTABLEISTUNG] = buff;
    sprintf(buff, "%d", setup.pid_powerWhichNeedNotConsumed);
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
    sprintf(buff, "%d", setup.tempMaxAllowedInGrad);
    data[TEMP_AUSSCHALTEN] = buff;
    sprintf(buff, "%d", setup.tempMinInGrad);
    data[TEMP_EINSCHALT] = buff;
    sprintf(buff, "%.2f", setup.pid_p);
    data[PID_P] = buff;
    sprintf(buff, "%.2f", setup.pid_i);
    data[PID_I] = buff;
    sprintf(buff, "%.2f", setup.pid_d);
    data[PID_D] = buff;
    sprintf(buff,"%.2f", setup.additionalLoad);
    data[SIM_ADDITIONAL_LOAD]=buff;
    sprintf(buff, "%d", setup.exportWatt);
    data[SIM_BIAS_POWER] = buff;
    DBGf("ajaxCalls_handleGetSetup - return ");
    return returnFromStoreSetup(true, data, request);
}  

void ajaxCalls_handleStoreSetup(AsyncWebServerRequest *request, JsonVariant &json, bool isAPModus)
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
    DBGf("ajaxCalls_handleStoreSetup BEGIN - %s, %s", WLAN_ESSID, argument);

    errorH = util_isFieldFilled(WLAN_ESSID, argument, data);
    if (errorH)
        strcpy(setup.ssid, jsonObj[WLAN_ESSID]);
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[WLAN_PASSWD];
    DBGf("ARG: %s, VAl: %s", WLAN_PASSWD, argument);

    errorH = util_isFieldFilled(WLAN_PASSWD, argument, data);
    if (errorH)
        strcpy(setup.passwd, jsonObj[WLAN_PASSWD]);
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[IP_INVERTER];
    DBGf("ARG: %s, VAl: %s", IP_INVERTER, argument);

    errorH = util_isFieldFilled(IP_INVERTER, argument, data);
    if (errorH)
    {
        bool result = true;
        String ipInv = argument;
        if (!result)
        {
            DBGf("handleStoreSetup IP translate did not succeed for IP: %s ", argument);

            data["error"] = "IP-Transformation war nicht erfolgreich!";
            return returnFromStoreSetup(false, data, request);
        }
        else
            setup.ipInverter = ipv4_string_to_int(ipInv, &result);
    }
    else
        return returnFromStoreSetup(errorH, data, request);

    argument = jsonObj[HEIZSTABLEISTUNG];

    DBGf("ARG: %s, VAl: %s", HEIZSTABLEISTUNG, argument);
    errorH = util_checkParamInt(HEIZSTABLEISTUNG, argument, data, &result);
    if (errorH)
        setup.heizstab_leistung_in_watt = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[EINSPEISUNG_MUSS];

    DBGf("ARG: %s, VAl: %s", EINSPEISUNG_MUSS, argument);
    errorH = util_checkParamInt(EINSPEISUNG_MUSS, argument, data, &result);
    if (errorH)
        setup.pid_powerWhichNeedNotConsumed = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[MINDEST_LAUFZEIT_DIGITALER_OUT];

    DBGf("ARG: %s, VAl: %s", MINDEST_LAUFZEIT_DIGITALER_OUT, argument);
    errorH = util_checkParamInt(MINDEST_LAUFZEIT_DIGITALER_OUT, argument, data, &result);
    if (errorH)
        setup.pid_min_time_before_switch_off_channel_inMS = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[MINDEST_LAUFZEIT_PORT_ON];

    DBGf("ARG: %s, VAl: %s", MINDEST_LAUFZEIT_PORT_ON, argument);
    errorH = util_checkParamInt(MINDEST_LAUFZEIT_PORT_ON, argument, data, &result);
    if (errorH)
        setup.pid_min_time_for_dig_output_inMS = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[MINDEST_LAUFZEIT_REGLER_KONSTANT];

    DBGf("ARG: %s, VAl: %s", MINDEST_LAUFZEIT_REGLER_KONSTANT, argument);
    errorH = util_checkParamInt(MINDEST_LAUFZEIT_REGLER_KONSTANT, argument, data, &result);
    if (errorH)
        setup.pid_min_time_without_contoller_inMS = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[EXTERNER_SPEICHER];
    DBGf("ARG: %s, VAl: %s", EXTERNER_SPEICHER, argument);
    if (util_isFieldFilled(EXTERNER_SPEICHER, argument, data))
    {
        setup.externerSpeicher = *argument == 'j' ? true : false;
    }
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[EXTERNER_SPEICHER_PRIORI];

    DBGf("ARG: %s, VAl: %s", EXTERNER_SPEICHER_PRIORI, argument);
    if (util_isFieldFilled(EXTERNER_SPEICHER_PRIORI, argument, data))
    {
        setup.externerSpeicherPriori = *argument; // j1|2|3
    }
    else
        return returnFromStoreSetup(errorH, data, request);

    argument = jsonObj[TEMP_AUSSCHALTEN];
    DBGf("ARG: %s, VAl: %s", TEMP_AUSSCHALTEN, argument);
    errorH = util_checkParamInt(TEMP_AUSSCHALTEN, argument, data, &result);
    if (errorH)
        setup.tempMaxAllowedInGrad = result;
    else
        return returnFromStoreSetup(errorH, data, request);

    argument = jsonObj[TEMP_EINSCHALT];
    DBGf("ARG: %s, VAl: %s", TEMP_EINSCHALT, argument);
    errorH = util_checkParamInt(TEMP_EINSCHALT, argument, data, &result);
    if (errorH)
        setup.tempMinInGrad = result;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[PID_P];
    DBGf("ARG: %s, VAl: %s", PID_P, argument);
    errorH = util_checkParamFloat(PID_P, argument, data, &resultF);
    if (errorH)
        setup.pid_p = resultF;
    argument = jsonObj[PID_I];
    DBGf("ARG: %s, VAl: %s", PID_I, argument);
    errorH = util_checkParamFloat(PID_I, argument, data, &resultF);
    if (errorH)
        setup.pid_i = resultF;
    else
        return returnFromStoreSetup(errorH, data, request);
    argument = jsonObj[PID_D];
    DBGf("ARG: %s, VAl: %s", PID_D, argument);
    errorH = util_checkParamFloat(PID_D, argument, data, &resultF);
    if (errorH)
        setup.pid_d = resultF;
    else
        return returnFromStoreSetup(errorH, data, request);


  argument = jsonObj[SIM_ADDITIONAL_LOAD];
    DBGf("ARG: %s, VAl: %s", SIM_ADDITIONAL_LOAD, argument);
    errorH = util_checkParamFloat(SIM_ADDITIONAL_LOAD, argument, data, &resultF);
    if (errorH)
        setup.additionalLoad = resultF;


    argument = jsonObj[SIM_BIAS_POWER];
    DBGf("ARG: %s, VAl: %s", SIM_BIAS_POWER, argument);
    errorH = util_checkParamInt(SIM_BIAS_POWER, argument, data, &result);
    if (errorH)
        setup.exportWatt = result;
    DBGf("ajaxCalls_handleStoreSetup END - RESTART after 10 s");

    eprom_storeSetup(setup);
    eprom_test_read_Eprom();
    returnFromStoreSetup(errorH, data, request);
    if (isAPModus)
    {
        DBGf("ajaxCalls::store - in AP-Modus a restart is required");
        delay(10000); // wait 10 secs
        esp_restart();
    }
}
/* private functions */

static void returnFromStoreSetup(bool inputCorrect, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, AsyncWebServerRequest *request)
{
    String response;
    if (inputCorrect)
    {
        data["done"] = 1;
        data["error"] = "";
        DBGf("returnFromStoreSetup - no errors ");
        // eprom_storeSetup(setup);
    }
    else
    {
        data["done"] = 0;
        DBGf("returnFromStoreSetup -  errors ");
    }

    serializeJson(data, response);
    DBGf("returnFromStoreSetup - return ");
    request->send(200, "application/json", response);
}
