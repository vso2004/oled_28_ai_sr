set(LV_DEMO_DIR "")
set(LV_DEMOS_SOURCES "")

#list(APPEND LV_DEMO_DIR ../components/lvgl__lvgl/demos)
#file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

#set_source_files_properties(
#    ${LV_DEMOS_SOURCES}
#    PROPERTIES COMPILE_OPTIONS
#    -DLV_LVGL_H_INCLUDE_SIMPLE)

# ${LV_DEMOS_SOURCES}

idf_component_register(
    SRCS main.c mic_speech.c
    INCLUDE_DIRS "." ${LV_DEMO_DIR}
    REQUIRES oled_28_ai lvgl__lvgl espressif__esp-sr espressif__esp-dsp rgb_led_driver espressif__led_strip
    )

