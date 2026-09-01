#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

led_strip_handle_t configure_led(void);
void RGB_init();
void Set_RGB_led(uint8_t programm);
void app_main_ws2812(void);