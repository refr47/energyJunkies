#pragma once

/*ESP32S3*/
#define PIN_LCD_BL 38

#define PIN_LCD_D0 39
#define PIN_LCD_D1 40
#define PIN_LCD_D2 41
#define PIN_LCD_D3 42
#define PIN_LCD_D4 45
#define PIN_LCD_D5 46
#define PIN_LCD_D6 47
#define PIN_LCD_D7 48

#define PIN_POWER_ON 15

#define PIN_LCD_RES 5
#define PIN_LCD_CS 6
#define PIN_LCD_DC 7
#define PIN_LCD_WR 8
#define PIN_LCD_RD 9

#define PIN_BUTTON_1 0
#define PIN_BUTTON_2 14
#define PIN_BAT_VOLT 4

#define PIN_IIC_SCL 17
#define PIN_IIC_SDA 18

#define PIN_TOUCH_INT 16
#define PIN_TOUCH_RES 21

#define CARDREADER_SPI_CS 10
#define CARDREADER_SPI_MOSI 11
#define CARDREADER_SPI_CLK 12
#define CARDREADER_SPI_MISO 13

#ifdef ESP_S3
#define ONE_WIRE_TEMP_GPIO 5
#define PWM_FOR_PID 1
#define SWITCH_KEY_UP 21
#define SWITCH_KEY_DOWN 22
#define SWITCH_KEY_ESC 18
#define SWITCH_KEY_ENTER 17
#define RELAY_L1 16
#define RELAY_L2 21
#else
#define ONE_WIRE_TEMP_GPIO 2
#define PWM_FOR_PID 1
#define SWITCH_KEY_UP 43
#define SWITCH_KEY_DOWN 44
#define SWITCH_KEY_ESC 18
#define SWITCH_KEY_ENTER 17
#define RELAY_L1 16
#define RELAY_L2 21
#endif