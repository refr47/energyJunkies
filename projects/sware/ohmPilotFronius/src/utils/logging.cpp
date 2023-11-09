
#include "logging.h"
#include "esp32-hal-log.h"
#include "cardRW.h"

// C:\users\throne\.platformio\packages\framework-arduinoespressif32\cores\esp32\esp32-hal-log.h
//https://community.platformio.org/t/redirect-esp32-log-messages-to-sd-card/33734/5
void logging_init() {

Serial.println("Setting log levels and callback");
		esp_log_level_set("TEST", LOG_LEVEL);

}