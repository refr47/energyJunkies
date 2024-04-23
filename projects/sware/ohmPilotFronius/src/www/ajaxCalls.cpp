#include "ajaxCalls.h"
#include "eprom.h"
#include "utils.h"

/*
    *******************  PROTOTYPES

*/

static CALLBACK_GET_DATA webSockData = NULL;
static void returnFromStoreSetup(bool inputCorrect, StaticJsonDocument<JSON_OBJECT_SETUP_LEN> &data, AsyncWebServerRequest *request);

void ajaxCalls_init(CALLBACK_GET_DATA getData)
{
    webSockData = getData;
}

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
    data[IP_INVERTER] = setup.inverter;

    sprintf(buff, "%d", setup.heizstab_leistung_in_watt);
    data[HEIZSTABLEISTUNG] = buff;
    sprintf(buff, "%d", setup.pid_powerWhichNeedNotConsumed);
    data[EINSPEISUNG_MUSS] = buff;

    /*  sprintf(buff, "%d", setup.pid_min_time_before_switch_off_channel_inMS);
     data[MINDEST_LAUFZEIT_DIGITALER_OUT] = buff;
     */
    sprintf(buff, "%d", setup.pid_min_time_without_contoller_inMS);
    data[MINDEST_LAUFZEIT_REGLER_KONSTANT] = buff;

    /*   sprintf(buff, "%d", setup.pid_min_time_for_dig_output_inMS);
      data[MINDEST_LAUFZEIT_REGLER_KONSTANT] = buff;
    */
    data[EXTERNER_SPEICHER] = setup.externerSpeicher ? "j'" : "n";
    sprintf(buff, "%c", setup.externerSpeicherPriori);
    data[EXTERNER_SPEICHER_PRIORI] = buff;
    sprintf(buff, "%d", setup.tempMaxAllowedInGrad);
    data[TEMP_AUSSCHALTEN] = buff;
    sprintf(buff, "%d", setup.tempMinInGrad);
    data[TEMP_EINSCHALT] = buff;
    /*   mqtt*/
    sprintf(buff, "%s", setup.mqttHost);
    data[WWW_MQTT_HOST] = buff;
    sprintf(buff, "%s", setup.mqttUser);
    data[WWW_MQTT_USER] = buff;
    sprintf(buff, "%s", setup.mqttPass);
    data[WWW_MQTT_PASWWD] = buff;

    /* INflux*/

    sprintf(buff, "%s", setup.influxHost);
    data[WWW_INFLUX_HOST] = buff;
    sprintf(buff, "%s", setup.influxToken);
    data[WWW_INFLUX_TOKEN] = buff;
    sprintf(buff, "%s", setup.influxOrg);
    data[WWW_INFLUX_ORG] = buff;
    sprintf(buff, "%s", setup.influxBucket);
    data[WWW_INFLUX_BUCKET] = buff;
    /*  amis*/

    sprintf(buff, "%s", setup.amisReaderHost);
    data[AMIS_READER_HOST] = buff;
    DBGf("ajaxCalls_handleGetSetup - AMIS_READER_HOST: %s", buff);
    sprintf(buff, "%s", setup.amisKey);
    data[AMIS_READER_KEY] = buff;

    /*     sprintf(buff, "%.2f", setup.pid_p);
        data[PID_P] = buff;
        sprintf(buff, "%.2f", setup.pid_i);
        data[PID_I] = buff;
        sprintf(buff, "%.2f", setup.pid_d);
        data[PID_D] = buff; */

    sprintf(buff, "%.2f", setup.additionalLoad);
    data[SIM_ADDITIONAL_LOAD] = buff;
    sprintf(buff, "%d", setup.forceHeating);
    data[FORCE_HEIZPATRONE] = buff;

    DBGf("ajaxCalls_handleGetSetup - return ");
    return returnFromStoreSetup(true, data, request);
}

void ajaxCalls_handleGetOverview(AsyncWebServerRequest *request)
{
    char formatBuffer[50] = "";
    WEBSOCK_DATA webSockD = webSockData();
    StaticJsonDocument<JSON_OBJECT_SETUP_LEN> data;
    // char buff[50]   ;
    DBGf("ajaxCalls_handleGetOverview - begin");

    data[WWW_FRONIUS] = webSockD.states.froniusAPI;
    // sprintf(buff,"%s",webSockD.setupData.inverter);
    if (webSockD.states.froniusAPI)
    {
        data[WWW_FRONIUS_IP] = webSockD.setupData.inverter;
    }
    else
    {
        data[WWW_FRONIUS_IP] = "";
    }
    data[WWW_AMIS] = webSockD.states.amisReader;
    if (webSockD.states.amisReader)
    {
        data[WWW_AMIS_IP] = webSockD.setupData.amisReaderHost;
    }
    else
    {
        data[WWW_AMIS_IP] = "";
    }
    data[WWW_CARDREADER] = webSockD.states.cardWriterOK;
    data[WWW_AKKU] = webSockD.setupData.externerSpeicher;
    if (webSockD.setupData.externerSpeicher)
        data[WWW_AKKU_KAPA] = webSockD.fronius_SOLAR_POWERFLOW.p_akku;
    else
    {
        data[WWW_AKKU_KAPA] = 0.0;
    }

    data[WWW_FLASH] = webSockD.states.flashOK;
    data[WWW_INFLUX] = webSockD.states.influx;
    if (webSockD.states.influx)
    {
        data[WWW_INFLUX_IP] = webSockD.setupData.influxHost;
    }
    else
    {
        data[WWW_INFLUX_IP] = "";
    }
    data[WWW_MODBUS] = webSockD.states.modbusOK;

    if (webSockD.states.modbusOK)
    {
        data[WWW_MODBUS_IP] = webSockD.setupData.inverter;
    }
    else
    {
        data[WWW_MODBUS_IP] = "";
    }

    data[WWW_MQTT] = webSockD.states.mqtt;
    if (webSockD.states.mqtt)
    {
        data[WWW_MQTT_IP] = webSockD.setupData.mqttHost;
    }
    else
    {
        data[WWW_MQTT_IP] = "";
    }
    data[WWW_TEMP_SENSOR] = webSockD.states.tempSensorOK;
    if (webSockD.temperature.alarm)
    {
        if (webSockD.temperature.sensor1 > 0.0 && webSockD.temperature.sensor2 > 0.0)
        {
            sprintf(formatBuffer, "!!%.2f!!", (webSockD.temperature.sensor1 + webSockD.temperature.sensor2) / 2.0);
            // readings[TEMP_PUFFERSPEICHER] = (data.temperature.sensor1 + data.temperature.sensor2) / 2.0);
        }
        else
        {
            sprintf(formatBuffer, "!!%.2f %.2f", webSockD.temperature.sensor1, webSockD.temperature.sensor2);
        }
    }
    else
    {
        sprintf(formatBuffer, "%.2f", (webSockD.temperature.sensor1 + webSockD.temperature.sensor2) / 2.0);
    }
    data[WWW_TEMP_SENSOR_VAL] = formatBuffer;
    DBGf("ajaxCalls_handleGetOverview - return ");

    String response;

    data["done"] = 1;
    data["error"] = "";
    DBGf("returnFromStoreSetup - no errors ");
    // eprom_storeSetup(setup);

    serializeJson(data, response);
    DBGf("returnFromStoreSetup - return ");
    request->send(200, "application/json", response);
}

void ajaxCalls_handleStoreSetup(AsyncWebServerRequest *request, JsonVariant &json, bool isAPModus)
{
    const JsonObject &jsonObj = json.as<JsonObject>();
    StaticJsonDocument<JSON_OBJECT_SETUP_LEN> data;
    WEBSOCK_DATA webSockD = webSockData();
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
    webSockD.setupData.setupChanged = false;
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
        strncpy(setup.inverter, argument, INET_ADDRSTRLEN);
        /* bool result = true;

        setup.ipInverter = ipv4_string_to_int((char *)argument, &result);
        DBGf("Setup: IP-Transformation: %s - %d", argument, setup.ipInverter);
        if (!result)
        {
            DBGf("handleStoreSetup IP translate did not succeed for IP: %s ", argument);

            data["error"] = "IP-Transformation war nicht erfolgreich!";
            return returnFromStoreSetup(false, data, request);
        } */
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

    /*  argument = jsonObj[MINDEST_LAUFZEIT_DIGITALER_OUT];


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
    */

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
    /*            MQTT */
    argument = jsonObj[WWW_MQTT_HOST];
    DBGf("ARG: %s, VAl: %s", WWW_MQTT_HOST, argument);
    if (strlen(argument) < 3)
        argument = EMPTY_STRING;
    strncpy(setup.mqttHost, jsonObj[WWW_MQTT_HOST], MQTT_HOST_LEN);

    /*   errorH = util_isFieldFilled(WWW_MQTT_HOST, argument, data);
      if (errorH)
      {
          strncpy(setup.mqttHost, jsonObj[WWW_MQTT_HOST], MQTT_HOST_LEN);
      }
      else
          return returnFromStoreSetup(errorH, data, request); */

    argument = jsonObj[WWW_MQTT_PASWWD];
    DBGf("ARG: %s, VAl: %s", WWW_MQTT_PASWWD, argument);
    if (strlen(argument) < 3)
        argument = EMPTY_STRING;
    strncpy(setup.mqttPass, jsonObj[WWW_MQTT_PASWWD], MQTT_PASS_LEN);

    /*  errorH = util_isFieldFilled(WWW_MQTT_PASWWD, argument, data);
     if (errorH)
     {
         strncpy(setup.mqttPass, jsonObj[WWW_MQTT_PASWWD], MQTT_PASS_LEN);
     }
     else
         return returnFromStoreSetup(errorH, data, request); */

    argument = jsonObj[WWW_MQTT_USER];
    DBGf("ARG: %s, VAl: %s", WWW_MQTT_USER, argument);

    if (strlen(argument) < 3)
        argument = EMPTY_STRING;
    strncpy(setup.mqttUser, jsonObj[WWW_MQTT_USER], MQTT_USER_LEN);

    /*  errorH = util_isFieldFilled(WWW_MQTT_USER, argument, data);
     if (errorH)
     {
         strncpy(setup.mqttUser, jsonObj[WWW_MQTT_USER], MQTT_USER_LEN);
     }
     else
         return returnFromStoreSetup(errorH, data, request); */

    /*            influx */

    argument = jsonObj[WWW_INFLUX_HOST];
    DBGf("ARG: %s, VAl: %s", WWW_INFLUX_HOST, argument);

    if (strlen(argument) < 3)
        argument = EMPTY_STRING;
    strncpy(setup.influxHost, jsonObj[WWW_INFLUX_HOST], INFLUX_HOST_LEN);

    /* errorH = util_isFieldFilled(WWW_INFLUX_HOST, argument, data);
    if (errorH)
    {
        strncpy(setup.influxHost, jsonObj[WWW_INFLUX_HOST], INFLUX_HOST_LEN);
    }
    else
        return returnFromStoreSetup(errorH, data, request); */

    argument = jsonObj[WWW_INFLUX_TOKEN];
    DBGf("ARG: %s, VAl: %s", WWW_INFLUX_TOKEN, argument);

    if (strlen(argument) < 3)
        argument = EMPTY_STRING;
    strncpy(setup.influxToken, jsonObj[WWW_INFLUX_TOKEN], INFLUX_TOKEN_LEN);

    /*  errorH = util_isFieldFilled(WWW_INFLUX_TOKEN, argument, data);
     if (errorH)
     {
         strncpy(setup.influxToken, jsonObj[WWW_INFLUX_TOKEN], INFLUX_TOKEN_LEN);
     }
     else
         return returnFromStoreSetup(errorH, data, request); */

    argument = jsonObj[WWW_INFLUX_ORG];
    DBGf("ARG: %s, VAl: %s", WWW_INFLUX_ORG, argument);

    if (strlen(argument) < 3)
        argument = EMPTY_STRING;
    strncpy(setup.influxOrg, jsonObj[WWW_INFLUX_ORG], INFLUX_ORG_LEN);

    /* errorH = util_isFieldFilled(WWW_INFLUX_ORG, argument, data);
    if (errorH)
    {
        strncpy(setup.influxOrg, jsonObj[WWW_INFLUX_ORG], INFLUX_ORG_LEN);
    }
    else
        return returnFromStoreSetup(errorH, data, request); */

    argument = jsonObj[WWW_INFLUX_BUCKET];
    DBGf("ARG: %s, VAl: %s", WWW_INFLUX_BUCKET, argument);

    if (strlen(argument) < 3)
        argument = EMPTY_STRING;
    strncpy(setup.influxBucket, jsonObj[WWW_INFLUX_BUCKET], INFLUX_BUCKET_LEN);

    /*
        errorH = util_isFieldFilled(WWW_INFLUX_BUCKET, argument, data);
        if (errorH)
        {
            strncpy(setup.influxBucket, jsonObj[WWW_INFLUX_BUCKET], INFLUX_BUCKET_LEN);
        }
        else
            return returnFromStoreSetup(errorH, data, request); */

    /*
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
*/

    argument = jsonObj[AMIS_READER_HOST];
    DBGf("ARG: %s, VAl: %s", AMIS_READER_HOST, argument);
    if (strlen(argument) < 3)
        argument = EMPTY_STRING;
    strcpy(setup.amisReaderHost, argument);
    /* if (util_isFieldFilled(AMIS_READER_HOST, argument, data))
    {
        strcpy(setup.amisReaderHost, argument);
    }
    else
        return returnFromStoreSetup(errorH, data, request); */

    argument = jsonObj[AMIS_READER_KEY];
    DBGf("ARG: %s, VAl: %s", AMIS_READER_KEY, argument);
    errorH = util_isFieldFilled(AMIS_READER_KEY, argument, data);
    if (errorH)
    {
        strcpy(setup.amisKey, jsonObj[AMIS_READER_KEY]);
    }
    else
        return returnFromStoreSetup(errorH, data, request);

    argument = jsonObj[SIM_ADDITIONAL_LOAD];
    DBGf("ARG: %s, VAl: %s", SIM_ADDITIONAL_LOAD, argument);
    errorH = util_checkParamFloat(SIM_ADDITIONAL_LOAD, argument, data, &resultF);
    if (errorH)
        setup.additionalLoad = resultF;

    argument = jsonObj[FORCE_HEIZPATRONE];
    DBGf("ARG: %s, VAl: %s", FORCE_HEIZPATRONE, argument);
    errorH = util_checkParamInt(FORCE_HEIZPATRONE, argument, data, &result);
    if (errorH)
        setup.forceHeating = result;
    webSockD.setupData.setupChanged = true;
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
