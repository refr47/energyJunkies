#include "energieManager.h"

static uint32_t SLEEP_DURATION = 1000; // µs

void eM_setSleepTime(uint32_t time) // in secs
{
    SLEEP_DURATION = time * 1000;
}

// Enter Light Sleep with Timer Wake - up source
void eM_lightSleep()
{
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
    esp_light_sleep_start();
}

// Enter Deep Sleep with Timer Wake-up source
void eM_deepSleep()
{
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
    esp_deep_sleep_start();
}

// Enter Hibernation mode with Timer Wake-up source
void eM_hibernate()
{
    // Shut down RTC (Low Power) Peripherals
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    // Shut down RTC Slow Memory
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    // Shut down RTC Fast Memory
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    // Shut down Crystal Oscillator XTAL
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
    // Enter Deep Sleep
    eM_deepSleep();
}

void eM_printWakeUpReason()
{
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

    switch (wakeupReason)
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println("The device was awakened by an external signal using RTC_IO.");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println("The device was awakened by an external signal using RTC_CNTL.");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("The device was awakened by a timer event.");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println("The device was awakened by a touchpad interaction.");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        Serial.println("The device was awakened by a ULP (Ultra-Low-Power) program.");
        break;
    default:
        Serial.printf("The device woke up for an unknown reason: %d\n", wakeupReason);
        break;
    }
}