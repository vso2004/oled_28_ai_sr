  set(REQ esp_driver_i2c esp_driver_gpio esp_driver_i2s esp_driver_sdmmc esp_driver_sdspi  fatfs esp_lvgl_port )
  #set(TOUCH_REQ esp_lcd_touch_ft5x06 esp_lcd_touch_gt911 esp_lcd_touch_tt21100 esp_lcd_touch)
  set(TOUCH_REQ esp_lcd_touch_ft5x06 esp_lcd_touch)
  set(CODEC_REQ espressif__esp_codec_dev esp_codec_dev)
  set(LCD_REQ esp_lcd_ili9341 esp_lcd )
  set(PRIV_REQ esp_driver_spi esp_driver_ledc  esp_lcd espressif__button spiffs esp_psram     )

idf_component_register(
    SRCS "oled_28_ai_bsp.c" "oled_28_ai_bsp_audio.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES ${REQ}  ${TOUCH_REQ}  ${CODEC_REQ} ${LCD_REQ}
    PRIV_REQUIRES ${PRIV_REQ}  
)
