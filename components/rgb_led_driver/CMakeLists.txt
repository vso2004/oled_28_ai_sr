
set(SOURCES
    "rgb_led_driver.c" 
       
)

set(INCLUDE_DIRS 
    "."
)



idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    PRIV_REQUIRES esp_driver_rmt oled_28_ai rgb_led_driver espressif__led_strip
    #REQUIRES 
    #REQUIRES "driver" "  
)