
#include "logging.h"
#include "esp32-hal-log.h"
#include "cardRW.h" 

#define LOG_LEVEL ESP_LOG_INFO
#define MY_ESP_LOG_LEVEL ESP_LOG_WARN

// C:\users\throne\.platformio\packages\framework-arduinoespressif32\cores\esp32\esp32-hal-log.h
// https://community.platformio.org/t/redirect-esp32-log-messages-to-sd-card/33734/5
void logging_init()
{

	DBGf("Setting log levels and callback");
	esp_log_level_set("*", MY_ESP_LOG_LEVEL);
	esp_log_level_set(TAG, LOG_LEVEL);
	esp_log_set_vprintf(sdCardLogOutput);
	if (!cardRW_createLoggingFile()) {
		DBGf("Cannot create logging file on sd card");
	}
}
