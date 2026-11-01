#include "hotUpdate.h"
#include "eprom.h"
static bool updated = false;

bool hotUpdate(WEBSOCK_DATA &webSockData, PinManager &pidPinManager)
{

    LOG_DEBUG(TAG_HOT_UPDATE, "hotUpdate::hotUpdate - did something change %d ", webSockData.setupData.setupChanged);
    updated = false;
    Setup d;

    if (!eprom_getSetup(d)) {
        LOG_ERROR(TAG_HOT_UPDATE, "Cannot read setup; keeping running configuration");
        return true;
    }
    // Recreate network clients at boot, in their owning tasks, using persisted settings.
    if (strcmp(d.ssid, webSockData.setupData.ssid) || strcmp(d.passwd, webSockData.setupData.passwd)
        || memcmp(&d.mqttSetup, &webSockData.setupData.mqttSetup, sizeof(d.mqttSetup))
        || memcmp(&d.influxSetup, &webSockData.setupData.influxSetup, sizeof(d.influxSetup)))
        return false;
    vTaskDelay(pdMS_TO_TICKS(50)); // "Atempause"
 
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
    if (d.wattSetupForTest != webSockData.setupData.wattSetupForTest)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: wattSetupForTest changed !! - no reboot :: eprom: %d, web: %d", d.wattSetupForTest, webSockData.setupData.wattSetupForTest);
        webSockData.setupData.wattSetupForTest = d.wattSetupForTest;
        updated = true;
    }
    webSockData.setupData.epsilonML_PinManager = d.epsilonML_PinManager;
    webSockData.setupData.phasen_leistung_in_watt = d.phasen_leistung_in_watt;
    return true; // an unchanged, valid setup does not require a reboot
}
    // esp_restart();
