#include "hotUpdate.h"
#include "eprom.h"
static bool updated = false;

bool hotUpdate(WEBSOCK_DATA &webSockData, PinManager &pidPinManager)
{

    LOG_DEBUG(TAG_HOT_UPDATE, "hotUpdate::hotUpdate - did something change %d ", webSockData.setupData.setupChanged);
    updated = false;
    Setup d;

    eprom_getSetup(d);
    vTaskDelay(pdMS_TO_TICKS(50)); // "Atempause"
 
    if (strcmp(d.passwd, webSockData.setupData.passwd) != 0)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: passwd changed !! - no reboot :: eprom: %s, web: %s", d.passwd, webSockData.setupData.passwd);
        strncpy(webSockData.setupData.passwd, d.passwd, LEN_WLAN - 1);
        updated = true;
    }
    if (strcmp(d.inverter, webSockData.setupData.inverter) != 0)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: inverter changed !! - no reboot :: eprom: %s, web: %s", d.inverter, webSockData.setupData.inverter);
        strcpy(webSockData.setupData.inverter, d.inverter);
        updated = true;
    }
    if (strcmp(d.amisReaderHost, webSockData.setupData.amisReaderHost) != 0)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: amisReaderHost changed !! - no reboot :: eprom: %s, web: %s", d.amisReaderHost, webSockData.setupData.amisReaderHost);
        strcpy(webSockData.setupData.amisReaderHost, d.amisReaderHost);
        updated = true;
    }
    if (strcmp(d.amisKey, webSockData.setupData.amisKey) != 0)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: amisKey changed !! - no reboot :: eprom: %s, web: %s", d.amisKey, webSockData.setupData.amisKey);
        strcpy(webSockData.setupData.amisKey, d.amisKey);
        updated = true;
    }
    if (d.tempMaxAllowedInGrad != webSockData.setupData.tempMaxAllowedInGrad)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: tempMaxAllowedInGrad changed !! - no reboot :: eprom: %d, web: %d", d.tempMaxAllowedInGrad, webSockData.setupData.tempMaxAllowedInGrad);
        webSockData.setupData.tempMaxAllowedInGrad = d.tempMaxAllowedInGrad;
        updated = true;
    }
    // heizpatrone

    if (d.heizstab_leistung_in_watt != webSockData.setupData.heizstab_leistung_in_watt)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: heizstab_leistung_in_watt changed !! - no reboot :: eprom: %d, web: %d", d.heizstab_leistung_in_watt, webSockData.setupData.heizstab_leistung_in_watt);
        webSockData.setupData.heizstab_leistung_in_watt = d.heizstab_leistung_in_watt;
        updated = true;
    }
    if (d.tempMinInGrad != webSockData.setupData.tempMinInGrad)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: tempMinInGrad changed !! - no reboot :: eprom: %d, web: %d", d.tempMinInGrad, webSockData.setupData.tempMinInGrad);
        webSockData.setupData.tempMinInGrad = d.tempMinInGrad;
        updated = true;
    }
    if (d.legionellenDelta != webSockData.setupData.legionellenDelta)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: pid_powerWhichNeedNotConsumed changed !! - no reboot :: eprom: %d, web: %d", d.legionellenDelta, webSockData.setupData.legionellenDelta);
        webSockData.setupData.legionellenDelta = d.legionellenDelta;
        updated = true;
    }
    if (d.legionellenMaxTemp != webSockData.setupData.legionellenMaxTemp)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: legionellenMaxTemp changed !! - no reboot :: eprom: %d, web: %d", d.legionellenMaxTemp, webSockData.setupData.legionellenMaxTemp);
        webSockData.setupData.legionellenMaxTemp = d.legionellenMaxTemp;
        updated = true;
    }

    if (d.forceHeating != webSockData.setupData.forceHeating)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate::forceHeating changed !! - no reboot");
        LOG_DEBUG(TAG_HOT_UPDATE, "Old value: %d, new value: %d", webSockData.setupData.forceHeating, d.forceHeating);
        updated = true;
        /*
        webSockData.states.heating = HEATING_AUTOMATIC;


                switch (d.forceHeating)
                {
                case HEATING_AUTOMATIC:
                    pidPinManager.reset();
                    webSockData.states.heating = HEATING_AUTOMATIC;
                    break;
                case HEATING_ON_PHASE_1:
                    webSockData.states.heating = HEATING_ON_PHASE_1;
                    pidPinManager.reset();
                    // pidPinManager.switchOnL1();
                    break;
                case HEATING_ON_PHASE_1_2:
                    webSockData.states.heating = HEATING_ON_PHASE_1_2;
                    pidPinManager.reset();
                    // pidPinManager.switchOnL1();
                    // pidPinManager.switchOnL2();
                    break;
                case HEATING_ON_PHASE_1_2_3:
                    webSockData.states.heating = HEATING_ON_PHASE_1_2_3;
                    pidPinManager.reset();
                    break;
                case HEATING_OFF:
                    pidPinManager.reset();
                    webSockData.states.heating = HEATING_OFF;
                    break;

                default:
                    webSockData.states.heating = HEATING_AUTOMATIC;
                    break;
                }
                    */
        webSockData.setupData.forceHeating = d.forceHeating;
    }
    // heizstab_force_off (Heater Kill)
    if (d.forceHeatingStop != webSockData.setupData.forceHeatingStop)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate::forceHeatingStop changed !! - no reboot :: eprom: %d, web: %d", d.forceHeatingStop, webSockData.setupData.forceHeatingStop);
        webSockData.setupData.forceHeatingStop = d.forceHeatingStop;
        updated = true;
    }
    // akku
    if (d.akkuPriori != webSockData.setupData.akkuPriori)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate::akkuPriori changed !! - no reboot :: eprom: %c, web: %c", d.akkuPriori, webSockData.setupData.akkuPriori);
        webSockData.setupData.akkuPriori = d.akkuPriori;
        updated = true;
    }
    if (d.akku != webSockData.setupData.akku)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate::akku changed !! - no reboot :: eprom: %c, web: %c", d.akku, webSockData.setupData.akku);
        webSockData.setupData.akku = d.akku;
        updated = true;
    }
    // mqtt

    if (strcmp(d.mqttSetup.mqttHost, webSockData.setupData.mqttSetup.mqttHost) != 0)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: mqttHost changed !! - no reboot :: eprom: %s, web: %s", d.mqttSetup.mqttHost, webSockData.setupData.mqttSetup.mqttHost);
        strcpy(webSockData.setupData.mqttSetup.mqttHost, d.mqttSetup.mqttHost);
        updated = true;
    }
    if (strcmp(d.mqttSetup.mqttUser, webSockData.setupData.mqttSetup.mqttUser) != 0)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: mqttUser changed !! - no reboot :: eprom: %s, web: %s", d.mqttSetup.mqttUser, webSockData.setupData.mqttSetup.mqttUser);
        strcpy(webSockData.setupData.mqttSetup.mqttUser, d.mqttSetup.mqttUser);
        updated = true;
    }

    if (strcmp(d.mqttSetup.mqttPass, webSockData.setupData.mqttSetup.mqttPass) != 0)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: mqttPasswd changed !! - no reboot :: eprom: %s, web: %s", d.mqttSetup.mqttPass, webSockData.setupData.mqttSetup.mqttPass);
        strcpy(webSockData.setupData.mqttSetup.mqttPass, d.mqttSetup.mqttPass);
        updated = true;
    }

    // influx
    if (d.influxSetup.influxHost != webSockData.setupData.influxSetup.influxHost)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: influxHost changed !! - no reboot :: eprom: %s, web: %s", d.influxSetup.influxHost, webSockData.setupData.influxSetup.influxHost);
        strcpy(webSockData.setupData.influxSetup.influxHost, d.influxSetup.influxHost);
        updated = true;
    }
    if (d.influxSetup.influxToken != webSockData.setupData.influxSetup.influxToken)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: influxToken changed !! - no reboot :: eprom: %s, web: %s", d.influxSetup.influxToken, webSockData.setupData.influxSetup.influxToken);
        strcpy(webSockData.setupData.influxSetup.influxToken, d.influxSetup.influxToken);
        updated = true;
    }
    if (d.influxSetup.influxBucket != webSockData.setupData.influxSetup.influxBucket)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: influxBucket changed !! - no reboot :: eprom: %s, web: %s", d.influxSetup.influxBucket, webSockData.setupData.influxSetup.influxBucket);
        strcpy(webSockData.setupData.influxSetup.influxBucket, d.influxSetup.influxBucket);
        updated = true;
    }


    if (d.influxSetup.influxOrg != webSockData.setupData.influxSetup.influxOrg) {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: influxOrg changed !! - no reboot :: eprom: %s, web: %s", d.influxSetup.influxOrg, webSockData.setupData.influxSetup.influxOrg);
        strcpy(webSockData.setupData.influxSetup.influxOrg, d.influxSetup.influxOrg);
        updated = true;
    }


    if (d.influxSetup.influxToken != webSockData.setupData.influxSetup.influxToken) {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: influxToken changed !! - no reboot :: eprom: %s, web: %s", d.influxSetup.influxToken, webSockData.setupData.influxSetup.influxToken);
        strcpy(webSockData.setupData.influxSetup.influxToken, d.influxSetup.influxToken);
        updated = true;
    }
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: influxOrg changed !! - no reboot :: eprom: %s, web: %s", d.influxSetup.influxOrg, webSockData.setupData.influxSetup.influxOrg);
        strcpy(webSockData.setupData.influxSetup.influxOrg, d.influxSetup.influxOrg);
        updated = true;
    }
    if (d.influxSetup.influxToken != webSockData.setupData.influxSetup.influxToken)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: influxToken changed !! - no reboot :: eprom: %s, web: %s", d.influxSetup.influxToken, webSockData.setupData.influxSetup.influxToken);
        strcpy(webSockData.setupData.influxSetup.influxToken, d.influxSetup.influxToken);
        updated = true;
    }


    if (d.wattSetupForTest != webSockData.setupData.wattSetupForTest)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: wattSetupForTest changed !! - no reboot :: eprom: %d, web: %d", d.wattSetupForTest, webSockData.setupData.wattSetupForTest);
        webSockData.setupData.wattSetupForTest = d.wattSetupForTest;
        updated = true;
    }
    return updated;
}
    // esp_restart();
