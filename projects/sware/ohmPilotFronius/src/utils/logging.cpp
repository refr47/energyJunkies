
#include "logging.h"
#include "esp32-hal-log.h"
#include "cardRW.h"

void logging_init() {

Serial.println("Setting log levels and callback");
		esp_log_level_set("TEST", LOG_LEVEL);

}