#include "hotUpdate.h"
#include "eprom.h"
static bool updated = false;

bool hotUpdate(WEBSOCK_DATA &webSockData, PinManager &pidPinManager)
{

    DBGf("main::SETUP_CHECK_INTERVALL %d ", webSockData.setupData.setupChanged);
    updated = false;
    Setup d;

    eprom_getSetup(d);
    delay(10000); // wait 10 secs
    /* DBGf("==::== oldVal: %d, newVal: %d", webSockData.setupData.forceHeating, d.forceHeating); */
    if (d.forceHeating != webSockData.setupData.forceHeating)
    {
        DBGf("main::forceHeating changed !! - no reboot");
        DBGf("Old value: %d, new value: %d", webSockData.setupData.forceHeating, d.forceHeating);
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
            pidPinManager.switchOnL1();
            break;
        case HEATING_ON_PHASE_1_2:
            webSockData.states.heating = HEATING_ON_PHASE_1_2;
            pidPinManager.reset();
            pidPinManager.switchOnL1();
            pidPinManager.switchOnL2();
            break;
        case HEATING_ON_PHASE_1_2_3:
            webSockData.states.heating = HEATING_ON_PHASE_1_2_3;
            pidPinManager.reset();
            pidPinManager.switchOnL1();
            pidPinManager.switchOnL2();
            pidPinManager.switchOnL3();
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
    if (d.externerSpeicherPriori != webSockData.setupData.externerSpeicherPriori)
    {
        DBGf("main::externerSpeicherPriori changed !! - no reboot :: eprom: %c, web: %c", d.externerSpeicherPriori, webSockData.setupData.externerSpeicherPriori);
        webSockData.setupData.externerSpeicherPriori = d.externerSpeicherPriori;
        updated = true;
    }
    if (d.tempMaxAllowedInGrad != webSockData.setupData.tempMaxAllowedInGrad)
    {
        DBGf("main:: tempMaxAllowedInGrad changed !! - no reboot :: eprom: %d, web: %d", d.tempMaxAllowedInGrad, webSockData.setupData.tempMaxAllowedInGrad);
        webSockData.setupData.tempMaxAllowedInGrad = d.tempMaxAllowedInGrad;
        updated = true;
    }
    if (d.pid_powerWhichNeedNotConsumed != webSockData.setupData.pid_powerWhichNeedNotConsumed)
    {
        DBGf("main:: pid_powerWhichNeedNotConsumed changed !! - no reboot :: eprom: %d, web: %d", d.pid_powerWhichNeedNotConsumed, webSockData.setupData.pid_powerWhichNeedNotConsumed);
        webSockData.setupData.pid_powerWhichNeedNotConsumed = d.pid_powerWhichNeedNotConsumed;
        updated = true;
    }
    if (d.tempMinInGrad != webSockData.setupData.tempMinInGrad)
    {
        DBGf("main:: tempMinInGrad changed !! - no reboot :: eprom: %.3f, web: %d", d.tempMinInGrad, webSockData.setupData.tempMinInGrad);
        webSockData.setupData.tempMinInGrad = d.tempMinInGrad;
        updated = true;
    }
    if (d.additionalLoad != webSockData.setupData.additionalLoad)
    {
        DBGf("main:: additionalLoad changed !! - no reboot :: eprom: %.3f, web: %.3f", d.additionalLoad, webSockData.setupData.additionalLoad);
        webSockData.setupData.additionalLoad = d.additionalLoad;
        updated = true;
    }
    if (strcmp(d.passwd, webSockData.setupData.passwd) != 0)
    {
        DBGf("main:: passwd changed !! - no reboot :: eprom: %s, web: %s", d.passwd, webSockData.setupData.passwd);
        strncpy(webSockData.setupData.passwd, d.passwd, LEN_WLAN - 1);
        updated = true;
    }
    if (strcmp(d.inverter, webSockData.setupData.inverter) != 0)
    {
        DBGf("main:: inverter changed !! - no reboot :: eprom: %s, web: %s", d.inverter, webSockData.setupData.inverter);
        strcpy(webSockData.setupData.inverter, d.inverter);
        updated = true;
    }

    return updated;

    // esp_restart();
}