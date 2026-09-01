
//ESP32-S3 IO Pin define
/*

#define SD_SCK  38
#define SD_CMD 40
#define SD_D0 39
#define SD_D1 41
#define SD_D2 48
#define SD_D3 47

//I2S IO Pin define
#define I2S_MCK   4
#define I2S_BCK   5
#define I2S_DINT  6
#define I2S_DOUT  8
#define I2S_WS    7
#define I2S_NUM   I2S_NUM_1
#define AP_ENABLE 1


#define I2C_NUM           I2C_NUM_0      
#define I2C_SPEED         400000 
#define BSP_I2C_SCL           15       
#define BSP_I2C_SDA           16   
#define BSP_TOUCH_INT 17
#define BSP_TOUCH_RST 18

     

 #define TOUCH_FT6336
 #define TOUCH_FT6336_SCL 15
 #define TOUCH_FT6336_SDA 16
 #define TOUCH_FT6336_INT 17
 #define TOUCH_FT6336_RST 18
 #define TOUCH_MAP_X1 0
 #define TOUCH_MAP_X2 240
 #define TOUCH_MAP_Y1 0
 #define TOUCH_MAP_Y2 320

#define BSP_PIN_MOSI 11
#define BSP_PIN_MISO 13
#define BSP_PIN_SCK  12
#define BSP_PIN_CS_LCD 10
#define BSP_PIN_DC 46
#define BSP_PIN_BL 45

    


//pin usage as follow:
//            CS  DC/RS  RESET  SDI/MOSI  SCK  SDO/MISO  BL   CTP_INT  CTP_RST  CTP_SDA  CTP_SCL   VCC     GND    
//ESP32-S3:   10   46     -1      11      12      13     45     17       18       16       15       5V     GND  


*/

#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "esp_codec_dev.h"
//#include "iot_button.h"
#include "bsp/config.h"
#include "bsp/display.h"

#include "touch.h"
//#include "iot_sensor_hub.h"


#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "esp_task_wdt.h"


#define BSP_RGB_PIN 42

#define ADC_I2S_CHANNEL 1
#define BSP_NUM_CHANAL 1
#define BSP_SAMPLE_RATE 16000
#define BSP_PLAY_CHANAL_FORMAT 1
#define BSP_BITS_PER_CHANAL 16
#define BSP_RECV_BUF_SIZE 1024


#define BSP_SD_MOUNT_POINT "/sdcard"
#define BSP_SPIFFS_MOUNT_POINT "/spiffs"
#define BSP_SPIFFS_PARTITION_LABEL "storage"
#define BSP_SPIFFS_MAX_FILES 5
#define BSP_LCD_DRAW_BUF_HEIGHT 100
#define BSP_PLAYER_VOLUME 100

//#define BSP_I2C_NUM     1
#define BSP_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)
#define BSP_LCD_SPI_NUM            (SPI3_HOST)
#define BSP_SDSPI_HOST          (SPI2_HOST)

/**************************************************************************************************
 *  BSP Board Name
 **************************************************************************************************/

/** @defgroup boardname Board Name
 *  @brief BSP Board Name
 *  @{
 */
//#define BSP_BOARD_ESP_BOX_3
/** @} */ // end of boardname

/**************************************************************************************************
 *  BSP Capabilities
 **************************************************************************************************/

/** @defgroup capabilities Capabilities
 *  @brief BSP Capabilities
 *  @{
 */
#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_TOUCH          1
#define BSP_CAPS_BUTTONS        1
#define BSP_CAPS_AUDIO          1
#define BSP_CAPS_AUDIO_SPEAKER  1
#define BSP_CAPS_AUDIO_MIC      1
#define BSP_CAPS_SDCARD         1
#define BSP_CAPS_IMU            1
#define BSP_CAPS_HUMITURE       1
/** @} */ // end of capabilities

/**************************************************************************************************
 *  ESP-BOX pinout
 **************************************************************************************************/

/** @defgroup g01_i2c I2C
 *  @brief I2C BSP API
 *  @{
 */

#define BSP_I2C_NUM            I2C_NUM_0      
#define bsp_I2C_SPEED          400000 
#define BSP_I2C_SCL        15       
#define BSP_I2C_SDA        16   
#define BSP_TOUCH_INT      17
#define BSP_TOUCH_RST        (GPIO_NUM_NC)

//#define BSP_I2C_SCL           (GPIO_NUM_15)
//#define BSP_I2C_SDA           (GPIO_NUM_16)

#define BSP_I2C_DOCK_SCL      (GPIO_NUM_40)
#define BSP_I2C_DOCK_SDA      (GPIO_NUM_41)
/** @} */ // end of i2c

/** @defgroup g03_audio Audio
 *  @brief Audio BSP API
 *  @{
 */

 
 //I2S IO Pin define
#define BSP_I2S_NUM             I2S_NUM_0
#define BSP_I2S_MCLK        (GPIO_NUM_4)
#define BSP_I2S_SCLK        (GPIO_NUM_5)
#define BSP_I2S_DSIN        (GPIO_NUM_6)
#define BSP_I2S_DOUT        (GPIO_NUM_8)
#define BSP_I2S_LCLK        (GPIO_NUM_7)
#define BSP_POWER_AMP_IO    (GPIO_NUM_1)
#define BSP_MUTE_STATUS       (GPIO_NUM_NC)
//#define BSP_I2S_SCLK          (GPIO_NUM_17)
//#define BSP_I2S_MCLK          (GPIO_NUM_2)
//#define BSP_I2S_LCLK          (GPIO_NUM_45)
//#define BSP_I2S_DOUT          (GPIO_NUM_15) // To Codec ES8311
//#define BSP_I2S_DSIN          (GPIO_NUM_16) // From ADC ES7210
//#define BSP_POWER_AMP_IO      (GPIO_NUM_46)

/** @} */ // end of audio

/** @defgroup g04_display Display and Touch
 *  @brief Display BSP API
 *  @{
 */

#define I2S_CONFIG_DEFAULT(sample_rate, channel_fmt, bits_per_chan) { \
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate), \
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits_per_chan, channel_fmt), \
        .gpio_cfg = { \
            .mclk = BSP_I2S_MCLK, \
            .bclk = BSP_I2S_SCLK, \
            .ws   = BSP_I2S_LCLK, \
            .dout = BSP_I2S_DOUT, \
            .din  = BSP_I2S_DSIN, \
        }, \
    }


 
#define BSP_LCD_DATA0 11
#define BSP_PIN_MISO 13
#define BSP_LCD_PCLK  12
#define BSP_LCD_CS 10
#define BSP_LCD_DC 46
#define BSP_LCD_BACKLIGHT 45
 

#define BSP_LCD_RST           (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_INT     (GPIO_NUM_17)


#define BSP_USB_POS           (GPIO_NUM_20)
#define BSP_USB_NEG           (GPIO_NUM_19)
/** @} */ // end of usb

/** @defgroup g05_buttons Buttons
 *  @brief Buttons BSP API
 *  @{
 */
#define BSP_BUTTON_CONFIG_IO  (GPIO_NUM_NC)
#define BSP_BUTTON_MUTE_IO    (GPIO_NUM_NC)
/** @} */ // end of buttons

/** @defgroup g02_storage SD Card and SPIFFS
 *  @brief SPIFFS and SD card BSP API
 *  @{
 */
/* uSD card MMC */

/*
#define SD_SCK  38
#define SD_CMD 40
#define SD_D0 39
#define SD_D1 41
#define SD_D2 48
#define SD_D3 47
*/

#define BSP_SD_D0             (GPIO_NUM_39)
#define BSP_SD_D1             (GPIO_NUM_41)
#define BSP_SD_D2             (GPIO_NUM_48)
#define BSP_SD_D3             (GPIO_NUM_47)
#define BSP_SD_CMD            (GPIO_NUM_40)
#define BSP_SD_CLK            (GPIO_NUM_38)
#define BSP_SD_DET            (GPIO_NUM_NC)
#define BSP_SD_POWER          (GPIO_NUM_NC)

/* uSD card SPI */


#define BSP_SD_SPI_MISO       (GPIO_NUM_9)
#define BSP_SD_SPI_CS         (GPIO_NUM_10)
#define BSP_SD_SPI_MOSI       (GPIO_NUM_11)
#define BSP_SD_SPI_CLK        (GPIO_NUM_12)
/** @} */ // end of storage

/** @defgroup g00_pmod PMOD
 *  @brief PMOD
 *  @{
 */
/* PMOD */
/*
 * PMOD interface (peripheral module interface) is an open standard defined by Digilent Inc.
 * for peripherals used with FPGA or microcontroller development boards.
 *
 * ESP-BOX contains two double PMOD connectors, protected with ESD protection diodes.
 * Power pins are on 3.3V.
 *
 * Double PMOD Connectors on ESP-BOX-3 dock are labeled as follows:
 *      |------------|
 *      | IO1    IO5 |
 *      | IO2    IO6 |
 *      | IO3    IO7 |
 *      | IO4    IO8 |
 *      |------------|
 *      | GND    GND |
 *      | 3V3    3V3 |
 *      |------------|
 */
// #define BSP_PMOD1_IO1        GPIO_NUM_42
// #define BSP_PMOD1_IO2        BSP_USB_POS
// #define BSP_PMOD1_IO3        GPIO_NUM_39
// #define BSP_PMOD1_IO4        GPIO_NUM_40 // Intended for I2C SCL (pull-up NOT populated)
// #define BSP_PMOD1_IO5        GPIO_NUM_21
// #define BSP_PMOD1_IO6        BSP_USB_NEG
// #define BSP_PMOD1_IO7        GPIO_NUM_38
// #define BSP_PMOD1_IO8        GPIO_NUM_41 // Intended for I2C SDA (pull-up NOT populated)

// #define BSP_PMOD2_IO1        GPIO_NUM_13 // Intended for SPI2 Q (MISO)
// #define BSP_PMOD2_IO2        GPIO_NUM_9  // Intended for SPI2 HD (Hold)
// #define BSP_PMOD2_IO3        GPIO_NUM_12 // Intended for SPI2 CLK
// #define BSP_PMOD2_IO4        GPIO_NUM_44 // UART0 RX by default
// #define BSP_PMOD2_IO5        GPIO_NUM_10 // Intended for SPI2 CS
// #define BSP_PMOD2_IO6        GPIO_NUM_14 // Intended for SPI2 WP (Write-protect)
// #define BSP_PMOD2_IO7        GPIO_NUM_11 // Intended for SPI2 D (MOSI)
// #define BSP_PMOD2_IO8        GPIO_NUM_43 // UART0 TX by defaultf
/** @} */ // end of pmod




/** \addtogroup g05_buttons
 *  @brief BSP Buttons
 *  @{
 */
typedef enum {
    BSP_BUTTON_CONFIG = 0,
    BSP_BUTTON_MUTE,
    BSP_BUTTON_MAIN,
    BSP_BUTTON_NUM
} bsp_button_t;
/** @} */ // end of buttons

#ifdef __cplusplus
extern "C" {
#endif


const audio_codec_data_if_t *bsp_audio_get_codec_itf(void);
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);
void delay(uint32_t val);
esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_deinit(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);
esp_err_t bsp_spiffs_mount(void);
esp_err_t bsp_spiffs_unmount(void);

typedef struct {
    const esp_vfs_fat_sdmmc_mount_config_t *mount;
    sdmmc_host_t *host;
    union {
        const sdmmc_slot_config_t   *sdmmc;
        const sdspi_device_config_t *sdspi;
    } slot;
} bsp_sdcard_cfg_t;

esp_err_t bsp_sdcard_mount(void);
esp_err_t bsp_sdcard_unmount(void);
sdmmc_card_t *bsp_sdcard_get_handle(void);

/**
 * @brief Get SD card MMC host config
 *
 * @param slot SD card slot
 * @param config Structure which will be filled
 */
void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config);

/**
 * @brief Get SD card SPI host config
 *
 * @param slot SD card slot
 * @param config Structure which will be filled
 */
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config);

/**
 * @brief Get SD card MMC slot config
 *
 * @param slot SD card slot
 * @param config Structure which will be filled
 */
void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config);

/**
 * @brief Get SD card SPI slot config
 *
 * @param spi_host SPI host ID
 * @param config Structure which will be filled
 */
void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config);

/**
 * @brief Mount microSD card to virtual file system (MMC mode)
 *
 * @param cfg SD card configuration
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_sdmmc_mount was already called
 *      - ESP_ERR_NO_MEM if memory cannot be allocated
 *      - ESP_FAIL if partition cannot be mounted
 *      - other error codes from SDMMC or SPI drivers, SDMMC protocol, or FATFS drivers
 */
esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg);

/**
 * @brief Mount microSD card to virtual file system (SPI mode)
 *
 * @param cfg SD card configuration
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_sdmmc_mount was already called
 *      - ESP_ERR_NO_MEM if memory cannot be allocated
 *      - ESP_FAIL if partition cannot be mounted
 *      - other error codes from SDMMC or SPI drivers, SDMMC protocol, or FATFS drivers
 */
esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg);

/** @} */ // end of storage

/** \addtogroup g04_display
 *  @{
 */

/**************************************************************************************************
 *
 * LCD interface
 *
 * ESP-BOX is shipped with 2.4inch ST7789 display controller.
 * It features 16-bit colors, 320x240 resolution and capacitive touch controller.
 *
 * LVGL is used as graphics library. LVGL is NOT thread safe, therefore the user must take LVGL mutex
 * by calling bsp_display_lock() before calling and LVGL API (lv_...) and then give the mutex with
 * bsp_display_unlock().
 *
 * Display's backlight must be enabled explicitly by calling bsp_display_backlight_on()
 **************************************************************************************************/



/**
 * @brief BSP display configuration structure
 */
typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;  /*!< LVGL port configuration */
    uint32_t        buffer_size;    /*!< Size of the buffer for the screen in pixels */
    bool            double_buffer;  /*!< True, if should be allocated two buffers */
    struct {
        unsigned int buff_dma: 1;    /*!< Allocated LVGL buffer will be DMA capable */
        unsigned int buff_spiram: 1; /*!< Allocated LVGL buffer will be in PSRAM */
    } flags;
} bsp_display_cfg_t;

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @param cfg display configuration
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Get pointer to input device (touch, buttons, ...)
 *
 * @note The LVGL input device is initialized in bsp_display_start() function.
 *
 * @return Pointer to LVGL input device or NULL when not initialized
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. 0 will block indefinitely.
 * @return true  Mutex was taken
 * @return false Mutex was NOT taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_display_unlock(void);

/**
 * @brief Set display enter sleep mode
 *
 * All the display (LCD, backlight, touch) will enter sleep mode.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_SUPPORTED if this function is not supported by the panel
 */
esp_err_t bsp_display_enter_sleep(void);

/**
 * @brief Set display exit sleep mode
 *
 * All the display (LCD, backlight, touch) will exit sleep mode.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_SUPPORTED if this function is not supported by the panel
 */
esp_err_t bsp_display_exit_sleep(void);

int esp_get_feed_channel(void);

char *esp_get_input_format(void);

/**
 * @brief Rotate screen
 *
 * Display must be already initialized by calling bsp_display_start()
 *
 * @param[in] disp Pointer to LVGL display
 * @param[in] rotation Angle of the display rotation
 */
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation);





/** @} */ // end of display

/** \addtogroup g05_buttons
 *  @{
 */

/**************************************************************************************************
 *
 * Button
 *
 * There are three buttons on ESP-BOX:
 *  - Reset:  Not programmable
 *  - Config: Controls boot mode during reset. Can be programmed after application starts
 *  - Mute:   This button is wired to Logic Gates and its result is mapped to GPIO_NUM_1
 **************************************************************************************************/

/**
 * @brief Initialize all buttons
 *
 * Returned button handlers must be used with espressif/button component API
 *
 * @note For LCD panel button which is defined as BSP_BUTTON_MAIN, bsp_display_start should
 *       be called before call this function.
 *
 * @param[out] btn_array      Output button array
 * @param[out] btn_cnt        Number of button handlers saved to btn_array, can be NULL
 * @param[in]  btn_array_size Size of output button array. Must be at least BSP_BUTTON_NUM
 * @return
 *     - ESP_OK               All buttons initialized
 *     - ESP_ERR_INVALID_ARG  btn_array is too small or NULL
 *     - ESP_FAIL             Underlying iot_button_create failed
 */
//esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size);

/** @} */ // end of buttons


/** @defgroup g10_sensors Sensors
 *  @brief BSP API for sensors
 *  @{
 */

/**
 * @brief BSP sensor configuration structure
 */
// typedef struct {
//     sensor_type_t type;
//     sensor_mode_t mode;
//     uint16_t period;
// } bsp_sensor_config_t;

/**
 * @brief Initialize a sensor
 *
 * @param[in]  cfg              Pointer to the sensor configuration
 * @param[out] sensor_handle    Pointer to the outgoing sensor handle
 * @return
 *     - ESP_OK on success, otherwise returns ESP_ERR_xxx
 */
//esp_err_t bsp_sensor_init(const bsp_sensor_config_t *cfg, sensor_handle_t *sensor_handle);

/** @} */ // end of sensors




extern i2s_chan_handle_t tx_handle; // I2S tx channel handler
extern  i2s_chan_handle_t rx_handle; // I2S rx channel handler

esp_err_t esp_task_wdt_stop(void);

esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_deinit(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);
esp_err_t bsp_i2c_device_probe(uint8_t addr);
esp_err_t bsp_spiffs_mount(void);
esp_err_t bsp_spiffs_unmount(void);
sdmmc_card_t *bsp_sdcard_get_handle(void);
void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config);
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config);
void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config);
void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config);
esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg);
esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg);
esp_err_t bsp_sdcard_mount(void);
esp_err_t bsp_sdcard_unmount(void);
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
esp_err_t bsp_display_brightness_init(void);
esp_err_t bsp_display_brightness_set(int brightness_percent);
esp_err_t bsp_display_backlight_off(void);
esp_err_t bsp_display_backlight_on(void);
esp_err_t bsp_lcd_enter_sleep(void);
esp_err_t bsp_lcd_exit_sleep(void);
lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg);
__attribute__((weak)) esp_err_t esp_lcd_touch_enter_sleep(esp_lcd_touch_handle_t tp);
__attribute__((weak)) esp_err_t esp_lcd_touch_exit_sleep(esp_lcd_touch_handle_t tp);
esp_err_t bsp_touch_enter_sleep(void);
esp_err_t bsp_touch_exit_sleep(void);
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch);
lv_indev_t *bsp_display_indev_init(lv_display_t *disp);
lv_display_t *bsp_display_start(void);
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);
lv_indev_t *bsp_display_get_input_dev(void);
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation);
bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);
esp_err_t bsp_display_enter_sleep(void);
esp_err_t bsp_display_exit_sleep(void);




esp_err_t esp_audio_play( const int16_t* data , int length , uint32_t ticks_to_wait );
esp_err_t esp_get_feed_data( bool is_get_raw_channel , int16_t* buffer , int buffer_len );
int esp_get_feed_channel( void );
char* esp_get_input_format( void );
uint32_t calc_rms(int16_t *dat, int len);
void I2S_Loop(void);
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);
const audio_codec_data_if_t *bsp_audio_get_codec_itf(void);
esp_err_t bsp_i2s_deinit(int i2s_num);
esp_err_t bsp_codec_init(int adc_sample_rate, int dac_sample_rate, int dac_channel_format, int dac_bits_per_chan);
esp_err_t bsp_codec_deinit();
esp_err_t esp_board_init_28ai(uint32_t sample_rate, int channel_format, int bits_per_chan);
esp_codec_dev_handle_t esp_ret_play_dev(void);
i2c_master_bus_handle_t esp_ret_i2c_handle(void);
esp_err_t bsp_codec_adc_init(int sample_rate, int adc_channel_format, int adc_bits_per_chan);
esp_err_t bsp_codec_dac_init(int sample_rate, int channel_format, int bits_per_chan);
esp_err_t bsp_codec_adc_deinit();
esp_err_t bsp_codec_dac_deinit();
esp_err_t esp_audio_set_play_vol(int volume);
esp_err_t esp_audio_get_play_vol(int *volume);
esp_err_t bsp_i2s_init(int i2s_num, uint32_t sample_rate, int channel_format, int bits_per_chan);
esp_err_t esp_board_init(uint32_t sample_rate, int channel_format, int bits_per_chan);


#ifdef __cplusplus
}
#endif
