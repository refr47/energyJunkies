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
    /* DBGf("==::== oldVal: %d, newVal: %d", webSockData.setupData.forceHeating, d.forceHeating); */
    if (d.forceHeating != webSockData.setupData.forceHeating)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate::forceHeating changed !! - no reboot");
        LOG_DEBUG(TAG_HOT_UPDATE, "Old value: %d, new value: %d", webSockData.setupData.forceHeating, d.forceHeating);
        webSockData.states.heating = HEATING_AUTOMATIC;
        updated = true;

        switch (d.forceHeating)
        {
        case HEATING_AUTOMATIC:
            pidPinManager.reset();
            webSockData.states.heating = HEATING_AUTOMATIC;
            break;
        case HEATING_ON_PHASE_1:
            webSockData.states.heating = HEATING_ON_PHASE_1;
            pidPinManager.reset();
            //pidPinManager.switchOnL1();
            break;
        case HEATING_ON_PHASE_1_2:
            webSockData.states.heating = HEATING_ON_PHASE_1_2;
            pidPinManager.reset();
            //pidPinManager.switchOnL1();
            //pidPinManager.switchOnL2();
            break;
        case HEATING_ON_PHASE_1_2_3:
            webSockData.states.heating = HEATING_ON_PHASE_1_2_3;
            pidPinManager.reset();
            //pidPinManager.switchOnL1();
            //pidPinManager.switchOnL2();
            //pidPinManager.switchOnL3();
            break;
        case HEATING_OFF:
            pidPinManager.reset();
            webSockData.states.heating = HEATING_OFF;
            break;

        default:
            webSockData.states.heating = HEATING_AUTOMATIC;
            break;
        }
        webSockData.setupData.forceHeating = d.forceHeating;
    }
    if (d.akkuPriori != webSockData.setupData.akkuPriori)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate::akkuPriori changed !! - no reboot :: eprom: %c, web: %c", d.akkuPriori, webSockData.setupData.akkuPriori);
        webSockData.setupData.akkuPriori = d.akkuPriori;
        updated = true;
    }
    if (d.tempMaxAllowedInGrad != webSockData.setupData.tempMaxAllowedInGrad)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: tempMaxAllowedInGrad changed !! - no reboot :: eprom: %d, web: %d", d.tempMaxAllowedInGrad, webSockData.setupData.tempMaxAllowedInGrad);
        webSockData.setupData.tempMaxAllowedInGrad = d.tempMaxAllowedInGrad;
        updated = true;
    }
    if (d.legionellenDelta != webSockData.setupData.legionellenDelta)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: pid_powerWhichNeedNotConsumed changed !! - no reboot :: eprom: %d, web: %d", d.legionellenDelta, webSockData.setupData.legionellenDelta);
        webSockData.setupData.legionellenDelta = d.legionellenDelta;
        updated = true;
    }
    if (d.tempMinInGrad != webSockData.setupData.tempMinInGrad)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: tempMinInGrad changed !! - no reboot :: eprom: %d, web: %d", d.tempMinInGrad, webSockData.setupData.tempMinInGrad);
        webSockData.setupData.tempMinInGrad = d.tempMinInGrad;
        updated = true;
    }
    if (d.wattSetupForTest != webSockData.setupData.wattSetupForTest)
    {
        LOG_INFO(TAG_HOT_UPDATE, "hotUpdate:: wattSetupForTest changed !! - no reboot :: eprom: %d, web: %d", d.wattSetupForTest, webSockData.setupData.wattSetupForTest);
        webSockData.setupData.wattSetupForTest = d.wattSetupForTest;
        updated = true;
    }
   /*  if (d.additionalLoad != webSockData.setupData.additionalLoad)
    {
        LOG_INFO("main:: additionalLoad changed !! - no reboot :: eprom: %.3f, web: %.3f", d.additionalLoad, webSockData.setupData.additionalLoad);
        webSockData.setupData.additionalLoad = d.additionalLoad;
        updated = true;
    } */
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

    return updated;

    // esp_restart();
}