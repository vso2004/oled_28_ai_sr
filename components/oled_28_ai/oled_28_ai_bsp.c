#include <string.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_spiffs.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_vfs_fat.h"

#include "bsp/oled_28_ai_bsp.h"
#include "bsp/display.h"
#include "bsp/touch.h"


#include "esp_lcd_touch_ft5x06.h"
#include "esp_lcd_ili9341.h"
#include "bsp_err_check.h"
#include "esp_codec_dev_defaults.h"

static const char *TAG = "oled_28_ai";

#define I2C_CLK_SPEED 400000

/** @cond */
_Static_assert(CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0, "Touch buttons must be supported for this BSP");
/** @endcond */

static lv_display_t *disp;
static lv_indev_t *disp_indev = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t tp;       // LCD touch handle
static sdmmc_card_t *bsp_sdcard = NULL; // Global uSD card handler
static bool spi_sd_initialized = false;

static i2c_master_bus_handle_t i2c_handle = NULL;
// static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static bool i2c_initialized = false;

int s_play_sample_rate = 16000;
int s_play_channel_format = 1;
int s_bits_per_chan = 16;

i2s_chan_handle_t tx_handle = NULL; // I2S tx channel handler
i2s_chan_handle_t rx_handle = NULL; // I2S rx channel handler

static audio_codec_data_if_t *record_data_if = NULL;
static audio_codec_ctrl_if_t *record_ctrl_if = NULL;
static audio_codec_if_t *record_codec_if = NULL;
static esp_codec_dev_handle_t record_dev = NULL;

static audio_codec_data_if_t *play_data_if = NULL;
static audio_codec_ctrl_if_t *play_ctrl_if = NULL;
static audio_codec_gpio_if_t *play_gpio_if = NULL;
static audio_codec_if_t *play_codec_if = NULL;
static esp_codec_dev_handle_t play_dev = NULL;

static const audio_codec_data_if_t *i2s_data_if = NULL; /* Codec data interface */
void delay(uint32_t val)
{
     vTaskDelay(pdMS_TO_TICKS(val));
}


esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized)
    {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_config = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_config, &i2c_handle));

    i2c_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
    i2c_initialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    bsp_i2c_init();
    return i2c_handle;
}

esp_err_t bsp_i2c_device_probe(uint8_t addr)
{
    return i2c_master_probe(i2c_handle, addr, 100);
}

esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = BSP_SPIFFS_MOUNT_POINT,
        .partition_label = BSP_SPIFFS_PARTITION_LABEL,
        .max_files = BSP_SPIFFS_MAX_FILES,
#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(BSP_SPIFFS_PARTITION_LABEL);
}

sdmmc_card_t *bsp_sdcard_get_handle(void)
{
    return bsp_sdcard;
}

void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config)
{
    assert(config);

    sdmmc_host_t host_config = SDMMC_HOST_DEFAULT();

    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config)
{
    assert(config);

    sdmmc_host_t host_config = SDSPI_HOST_DEFAULT();
    host_config.slot = slot;

    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config)
{
    assert(config);
    memset(config, 0, sizeof(sdmmc_slot_config_t));

    /* SD card is connected to Slot 0 pins. Slot 0 uses IO MUX, so not specifying the pins here */
    config->cd = SDMMC_SLOT_NO_CD;
    config->wp = SDMMC_SLOT_NO_WP;
    config->cmd = BSP_SD_CMD;
    config->clk = BSP_SD_CLK;
    config->d0 = BSP_SD_D0;
    config->d1 = BSP_SD_D1;
    config->d2 = BSP_SD_D2;
    config->d3 = BSP_SD_D3;
    config->width = 4;
    config->flags = 0;
}

void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config)
{
    assert(config);
    memset(config, 0, sizeof(sdspi_device_config_t));

    config->gpio_cs = BSP_SD_SPI_CS;
    config->gpio_cd = SDSPI_SLOT_NO_CD;
    config->gpio_wp = SDSPI_SLOT_NO_WP;
    config->gpio_int = GPIO_NUM_NC;
    config->host_id = spi_host;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    config->gpio_wp_polarity = SDSPI_IO_ACTIVE_LOW;
#endif
}

esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg)
{
    sdmmc_host_t sdhost = {0};
    sdmmc_slot_config_t sdslot = {0};
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    assert(cfg);

    // gpio_config_t power_gpio_config = {
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pin_bit_mask = 1ULL << BSP_SD_POWER};
    // ESP_ERROR_CHECK(gpio_config(&power_gpio_config));

    // /* SD card power on first */
    // ESP_ERROR_CHECK(gpio_set_level(BSP_SD_POWER, 0));

    if (!cfg->mount)
    {
        cfg->mount = &mount_config;
    }

    if (!cfg->host)
    {
        bsp_sdcard_get_sdmmc_host(SDMMC_HOST_SLOT_0, &sdhost);
        cfg->host = &sdhost;
    }

    if (!cfg->slot.sdmmc)
    {
        bsp_sdcard_sdmmc_get_slot(SDMMC_HOST_SLOT_0, &sdslot);
        cfg->slot.sdmmc = &sdslot;
    }

#if !CONFIG_FATFS_LONG_FILENAMES
    ESP_LOGW(TAG, "Warning: Long filenames on SD card are disabled in menuconfig!");
#endif

    return esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, cfg->host, cfg->slot.sdmmc, cfg->mount, &bsp_sdcard);
}

esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg)
{
    sdmmc_host_t sdhost = {0};
    sdspi_device_config_t sdslot = {0};
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    assert(cfg);

    // gpio_config_t power_gpio_config = {
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pin_bit_mask = 1ULL << BSP_SD_POWER};
    // ESP_ERROR_CHECK(gpio_config(&power_gpio_config));

    // /* SD card power on first */
    // ESP_ERROR_CHECK(gpio_set_level(BSP_SD_POWER, 0));

    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_SD_SPI_CLK,
        .mosi_io_num = BSP_SD_SPI_MOSI,
        .miso_io_num = BSP_SD_SPI_MISO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4000,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_SDSPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");
    spi_sd_initialized = true;

    if (!cfg->mount)
    {
        cfg->mount = &mount_config;
    }

    if (!cfg->host)
    {
        bsp_sdcard_get_sdspi_host(SDMMC_HOST_SLOT_0, &sdhost);
        cfg->host = &sdhost;
    }

    if (!cfg->slot.sdspi)
    {
        bsp_sdcard_sdspi_get_slot(BSP_SDSPI_HOST, &sdslot);
        cfg->slot.sdspi = &sdslot;
    }

#if !CONFIG_FATFS_LONG_FILENAMES
    ESP_LOGW(TAG, "Warning: Long filenames on SD card are disabled in menuconfig!");
#endif

    return esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, cfg->host, cfg->slot.sdspi, cfg->mount, &bsp_sdcard);
}

esp_err_t bsp_sdcard_mount(void)
{
    bsp_sdcard_cfg_t cfg = {0};
    return bsp_sdcard_sdmmc_mount(&cfg);
}

esp_err_t bsp_sdcard_unmount(void)
{
    esp_err_t ret = ESP_OK;

    ret |= esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
    bsp_sdcard = NULL;

    if (spi_sd_initialized)
    {
        ret |= spi_bus_free(BSP_SDSPI_HOST);
        spi_sd_initialized = false;
    }

    // gpio_reset_pin(BSP_SD_POWER);

    return ret;
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    bsp_i2c_init();
    bsp_audio_init(NULL);
    const audio_codec_data_if_t *i2s_data_if = bsp_audio_get_codec_itf();
    if (i2s_data_if == NULL)
    {
        /* Initilize I2C */
        // BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());
        /* Configure I2S peripheral and Power Amplifier */
        BSP_ERROR_CHECK_RETURN_ERR(bsp_audio_init(NULL));
        i2s_data_if = bsp_audio_get_codec_itf();
    }
    assert(i2s_data_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = true,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
        .mclk_div = 256,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    BSP_NULL_CHECK(es8311_dev, NULL);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    esp_codec_dev_handle_t *codec = esp_codec_dev_new(&codec_dev_cfg);
    esp_codec_dev_set_in_gain(codec, 42);
    int volume = 80;
    esp_codec_dev_set_out_vol(codec, volume);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = BSP_SAMPLE_RATE,
        .channel = BSP_NUM_CHANAL,
        .bits_per_sample = BSP_BITS_PER_CHANAL,
    };

    if (esp_codec_dev_open(codec, &fs) != ESP_CODEC_DEV_OK)
    {
        ESP_LOGE("audio_task", "Failed to open codec device");
        return 0;
    }

    return codec;
}

#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8
#define LCD_LEDC_CH CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH

esp_err_t bsp_display_brightness_init(void)
{
    // // Setup LEDC peripheral for PWM backlight control
    // const ledc_channel_config_t LCD_backlight_channel = {
    //     .gpio_num = BSP_LCD_BACKLIGHT,
    //     .speed_mode = LEDC_LOW_SPEED_MODE,
    //     .channel = LCD_LEDC_CH,
    //     .intr_type = LEDC_INTR_DISABLE,
    //     .timer_sel = 1,
    //     .duty = 0,
    //     .hpoint = 0
    // };
    // const ledc_timer_config_t LCD_backlight_timer = {
    //     .speed_mode = LEDC_LOW_SPEED_MODE,
    //     .duty_resolution = LEDC_TIMER_10_BIT,
    //     .timer_num = 1,
    //     .freq_hz = 5000,
    //     .clk_cfg = LEDC_AUTO_CLK
    // };

    // BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    // BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&LCD_backlight_channel));

    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(BSP_LCD_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(BSP_LCD_BACKLIGHT, 1);

    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    // if (brightness_percent > 100) {
    //     brightness_percent = 100;
    // }
    // if (brightness_percent < 0) {
    //     brightness_percent = 0;
    // }

    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(BSP_LCD_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(BSP_LCD_BACKLIGHT, 1);

    // gpio_config_t io_conf1 = {
    //     .pin_bit_mask = BIT64(BSP_LCD_RST),
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pull_up_en = GPIO_PULLUP_DISABLE,
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //     .intr_type = GPIO_INTR_DISABLE,
    // };
    // gpio_config(&io_conf1);
    // gpio_set_level(BSP_LCD_RST, 0);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // gpio_set_level(BSP_LCD_RST, 1);

    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

esp_err_t bsp_lcd_enter_sleep(void)
{
    assert(panel_handle);
    return esp_lcd_panel_disp_on_off(panel_handle, false);
}

esp_err_t bsp_lcd_exit_sleep(void)
{
    assert(panel_handle);
    return esp_lcd_panel_disp_on_off(panel_handle, true);
}

const ili9341_lcd_init_cmd_t vendor_specific_init2[] = {
    //  {cmd, { data }, data_size, delay_ms}
    /* Power control B */
    {0xCF, (uint8_t[]){0x00, 0xC1, 0x30}, 3, 0},
    /* Power on sequence control */
    {0xED, (uint8_t[]){0x64, 0x03, 0x12, 0x81}, 4, 0},
    /* Driver timing control A */
    {0xE8, (uint8_t[]){0x85, 0x00, 0x78}, 3, 0},
    /* Power control A */
    {0xCB, (uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5, 0},
    /* Pump ratio control */
    {0xF7, (uint8_t[]){0x20}, 1, 0},
    /* Driver timing control */
    {0xEA, (uint8_t[]){0x00, 0x00}, 2, 0},
    /* Power control 1 */
    {0xC0, (uint8_t[]){0x10}, 1, 0},
    /* Power control 2 */
    {0xC1, (uint8_t[]){0x00}, 1, 0},
    /* VCOM control 1 */
    {0xC5, (uint8_t[]){0x30, 0x30}, 2, 0},
    /* VCOM control 2 */
    {0xC7, (uint8_t[]){0xB7}, 1, 0},
    /* Frame rate control */
    {0xB1, (uint8_t[]){0x00, 0x1A}, 2, 0},
    /* Enable 3G */
    {0xF2, (uint8_t[]){0x00}, 1, 0},
    /* Gamma set */
    {0x26, (uint8_t[]){0x01}, 1, 0},
    /* Positive gamma correction */
    {0xE0, (uint8_t[]){0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15, 0}, // Adjusted for ILI9341_2_DRIVER
    /* Negative gamma correction */
    {0xE1, (uint8_t[]){0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15, 0}, // Adjusted for ILI9341_2_DRIVER
    /* Entry mode set */
    {0xB7, (uint8_t[]){0x07}, 1, 0},
    /* Display function control */
    {0xB6, (uint8_t[]){0x08, 0x82, 0x27}, 3, 0},
};

ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0xCF, (uint8_t[]){0x00, 0xC1, 0X30}, 3, 0},
    {0xED, (uint8_t[]){0x64, 0x03, 0X12, 0X81}, 4, 0},
    {0xE8, (uint8_t[]){0x85, 0x00, 0x78}, 3, 0},
    {0xCB, (uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5, 0},
    {0xF7, (uint8_t[]){0x20}, 1, 0},
    {0xEA, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xC0, (uint8_t[]){0x13}, 1, 0},       /*Power control*/
    {0xC1, (uint8_t[]){0x13}, 1, 0},       /*Power control */
    {0xC5, (uint8_t[]){0x22, 0x35}, 2, 0}, /*VCOM control*/
    {0xC7, (uint8_t[]){0xBD}, 1, 0},       /*VCOM control*/
    {0x21, (uint8_t[]){0}, 0, 0},
    {0x36, (uint8_t[]){0x08}, 1, 0}, /*Memory Access Control*/
    {0xB6, (uint8_t[]){0x0A, 0x82}, 2, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0}, /*Pixel Format Set*/
    {0xF6, (uint8_t[]){0x01, 0x30}, 2, 0},
    {0xB1, (uint8_t[]){0x00, 0x1B}, 2, 0},
    {0xF2, (uint8_t[]){0x00}, 1, 0},
    {0x26, (uint8_t[]){0x01}, 1, 0},
    {0xE0, (uint8_t[]){0x0F, 0x35, 0x31, 0x0B, 0x0F, 0x06, 0x49, 0XA7, 0x33, 0x07, 0x0F, 0x03, 0x0C, 0x0A, 0x00}, 15, 0},
    {0XE1, (uint8_t[]){0x00, 0x0A, 0x0F, 0x04, 0x11, 0x08, 0x36, 0x58, 0x4D, 0x07, 0x10, 0x0C, 0x32, 0x34, 0x0F}, 15, 0},
    {0x11, (uint8_t[]){0}, 0x80, 0},
    {0x29, (uint8_t[]){0}, 0x80, 0},
    {0, (uint8_t[]){0}, 0xff, 0},
};

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    assert(config != NULL && config->max_transfer_sz > 0);

    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");

    /* Initilize I2C */
    // BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_PCLK,
        .mosi_io_num = BSP_LCD_DATA0,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = config->max_transfer_sz,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_CS,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, ret_io), err, TAG,
                      "New panel IO failed");

    ESP_LOGD(TAG, "Install LCD driver");
    const ili9341_vendor_config_t vendor_config = {
        .init_cmds = &vendor_specific_init[0],
        .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST, // Shared with Touch reset
        .flags.reset_active_high = 1,
        .rgb_ele_order = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };

    panel_config.vendor_config = (void *)&vendor_config;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_ili9341(*ret_io, (const esp_lcd_panel_dev_config_t *)&panel_config, ret_panel), err,
                      TAG, "New panel failed");

    esp_lcd_panel_reset(*ret_panel);
    esp_lcd_panel_init(*ret_panel);
    esp_lcd_panel_mirror(*ret_panel, true, true);
    return ret;

err:
    if (*ret_panel)
    {
        esp_lcd_panel_del(*ret_panel);
    }
    if (*ret_io)
    {
        esp_lcd_panel_io_del(*ret_io);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
    return ret;
}

lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * BSP_LCD_DRAW_BUF_HEIGHT) * sizeof(uint16_t),
    };
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle));

    // esp_lcd_panel_disp_on_off(panel_handle, true);

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = cfg->buffer_size,
        .double_buffer = cfg->double_buffer,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = cfg->flags.buff_dma,
            .buff_spiram = cfg->flags.buff_spiram,
            // #if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = (BSP_LCD_BIGENDIAN ? true : false),
            // #endif
        }};

    return lvgl_port_add_disp(&disp_cfg);
}

__attribute__((weak)) esp_err_t esp_lcd_touch_enter_sleep(esp_lcd_touch_handle_t tp)
{
    ESP_LOGE(TAG, "Sleep mode not supported!");
    return ESP_FAIL;
}

__attribute__((weak)) esp_err_t esp_lcd_touch_exit_sleep(esp_lcd_touch_handle_t tp)
{
    ESP_LOGE(TAG, "Sleep mode not supported!");
    return ESP_FAIL;
}

esp_err_t bsp_touch_enter_sleep(void)
{
    assert(tp);
    return esp_lcd_touch_enter_sleep(tp);
}

esp_err_t bsp_touch_exit_sleep(void)
{
    assert(tp);
    return esp_lcd_touch_exit_sleep(tp);
}

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    /* Initilize I2C */
    // BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    /* Initialize touch */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = true,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;

    //

    // if (ESP_OK == bsp_i2c_device_probe(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS))
    // {
    //     esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    //     memcpy(&tp_io_config, &config, sizeof(config));
    // }
    // else if (ESP_OK == bsp_i2c_device_probe(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP))
    // {
    //     esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    //     config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
    //     memcpy(&tp_io_config, &config, sizeof(config));
    // }
    // else if (ESP_OK == bsp_i2c_device_probe(ESP_LCD_TOUCH_IO_I2C_TT21100_ADDRESS))
    // {
    //     esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_TT21100_CONFIG();
    //     memcpy(&tp_io_config, &config, sizeof(config));
    //     tp_cfg.flags.mirror_x = 1;
    // }
    // else if (ESP_OK == bsp_i2c_device_probe(ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS))
    // {
    //     esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    //     memcpy(&tp_io_config, &config, sizeof(config));
    //     tp_cfg.flags.mirror_x = 1;
    // }
    // else
    // {
    //     ESP_LOGE(TAG, "Touch not found");
    //     return ESP_ERR_NOT_FOUND;
    // }

        esp_lcd_panel_io_i2c_config_t tp_io_config =ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        //esp_lcd_panel_io_i2c_config_t config_FT5x06 = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
       // memcpy(&tp_io_config, &config_FT5x06, sizeof(config_FT5x06));
        tp_cfg.flags.mirror_x = 1;
    

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, ret_touch), TAG, "New ft5x06 failed");
    return ESP_OK;

    // if (ESP_LCD_TOUCH_IO_I2C_TT21100_ADDRESS == tp_io_config.dev_addr)
    // {
    //     ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_tt21100(tp_io_handle, &tp_cfg, ret_touch), TAG, "New tt21100 failed");
    //     return ESP_OK;
    // }
    // if (ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS == tp_io_config.dev_addr)
    // {
    //     ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch), TAG, "New gt911 failed");
    //     return ESP_OK;
    // }
    // if (ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP == tp_io_config.dev_addr)
    // {
    //     ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch), TAG, "New gt911 bacup failed");
    //     return ESP_OK;
    // }
    // if (ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS == tp_io_config.dev_addr)
    // {
    //     ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, ret_touch), TAG, "New ft5x06 failed");
    //     return ESP_OK;
    // }

    return -1;
}

lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(NULL, &tp));
    assert(tp);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp,
    };

    return lvgl_port_add_touch(&touch_cfg);
}

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = 1,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        }};
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg));

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_brightness_init());

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);

    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(disp), NULL);

    lv_obj_clean(lv_screen_active());
    delay(300);


    return disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}

esp_err_t bsp_display_enter_sleep(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(bsp_lcd_enter_sleep());
    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_backlight_off());
    BSP_ERROR_CHECK_RETURN_ERR(bsp_touch_enter_sleep());
    return ESP_OK;
}

esp_err_t bsp_display_exit_sleep(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(bsp_lcd_exit_sleep());
    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_backlight_on());
    BSP_ERROR_CHECK_RETURN_ERR(bsp_touch_exit_sleep());
    return ESP_OK;
}

// audio

esp_err_t esp_audio_play(const int16_t *data, int length, uint32_t ticks_to_wait)
{
    // size_t bytes_write = 0;
    esp_err_t ret = ESP_OK;
    if (!tx_handle)
    {
        return ESP_FAIL;
    }

    int out_length = length;
    int audio_time = 1;
    audio_time *= (16000 / BSP_SAMPLE_RATE);
    audio_time *= (2 / BSP_PLAY_CHANAL_FORMAT);

    int *data_out = NULL;
    if (BSP_BITS_PER_CHANAL != 32)
    {
        out_length = length * 2;
        data_out = malloc(out_length);
        for (int i = 0; i < length / sizeof(int16_t); i++)
        {
            int ret = data[i];
            data_out[i] = ret << 16;
        }
    }

    int *data_out_1 = NULL;
    if (BSP_PLAY_CHANAL_FORMAT != 2 || BSP_SAMPLE_RATE != 16000)
    {
        out_length *= audio_time;
        data_out_1 = malloc(out_length);
        int *tmp_data = NULL;
        if (data_out != NULL)
        {
            tmp_data = data_out;
        }
        else
        {
            tmp_data = (int *)data;
        }

        for (int i = 0; i < out_length / (audio_time * sizeof(int)); i++)
        {
            for (int j = 0; j < audio_time; j++)
            {
                data_out_1[audio_time * i + j] = tmp_data[i];
            }
        }
        if (data_out != NULL)
        {
            free(data_out);
            data_out = NULL;
        }
    }

    if (data_out != NULL)
    {
        ret = esp_codec_dev_write(tx_handle, (void *)data_out, out_length);
        free(data_out);
    }
    else if (data_out_1 != NULL)
    {
        ret = esp_codec_dev_write(tx_handle, (void *)data_out_1, out_length);
        free(data_out_1);
    }
    else
    {
        ret = esp_codec_dev_write(tx_handle, (void *)data, length);
    }

    return ret;
}

esp_err_t esp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len)
{
    esp_err_t ret = ESP_OK;
    // size_t bytes_read;
    int audio_chunksize = buffer_len / (sizeof(int16_t) * ADC_I2S_CHANNEL);

    ret = esp_codec_dev_read(rx_handle, (void *)buffer, buffer_len);
    if (!is_get_raw_channel)
    {
        for (int i = 0; i < audio_chunksize; i++)
        {
            int16_t ref = buffer[4 * i + 0];
            buffer[3 * i + 0] = buffer[4 * i + 1];
            buffer[3 * i + 1] = buffer[4 * i + 3];
            buffer[3 * i + 2] = ref;
        }
    }

    return ret;
}

int esp_get_feed_channel(void)
{
    return ADC_I2S_CHANNEL;
}

char *esp_get_input_format(void)
{
    return "RMNM";
}

uint32_t calc_rms(int16_t *dat, int len)
{
    uint32_t rms = 0;
    uint32_t rms_max = 0;

    for (int i = 0; i < len; i += 2)
    {
        if (rms_max < dat[i] * dat[i])
            rms_max = dat[i] * dat[i];

        rms += dat[i] * dat[i];
    }
    // if(rms_max > 2048)
    // ESP_LOGE("", "rms_max, %ld\n",rms_max);

    rms = rms / len;
    return rms;
}

void I2S_Loop(void)
{

    int16_t *mic_data1 = malloc(16384);
    size_t bytes_read = 0;
    size_t bytes_write = 0;
    i2s_channel_read(rx_handle, mic_data1, 256, &bytes_read, 1000);
    i2s_channel_write(tx_handle, mic_data1, 256, &bytes_write, 1000);
}
/* Can be used for i2s_std_gpio_config_t and/or i2s_std_config_t initialization */
#define BSP_I2S_GPIO_CFG       \
    {                          \
        .mclk = BSP_I2S_MCLK,  \
        .bclk = BSP_I2S_SCLK,  \
        .ws = BSP_I2S_LCLK,    \
        .dout = BSP_I2S_DOUT,  \
        .din = BSP_I2S_DSIN,   \
        .invert_flags = {      \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv = false,   \
        },                     \
    }

#define I2S_STD_PHILIPS_SLOT_VSO(bits_per_sample, mono_or_stereo, left_right) { \
    .data_bit_width = bits_per_sample,                                          \
    .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,                                  \
    .slot_mode = mono_or_stereo,                                                \
    .slot_mask = left_right,                                                    \
    .ws_width = bits_per_sample,                                                \
    .ws_pol = false,                                                            \
    .bit_shift = true,                                                          \
    .left_align = true,                                                         \
    .big_endian = false,                                                        \
    .bit_order_lsb = false}

/* This configuration is used by default in bsp_audio_init() */
#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }

#define BSP_I2S_DUPLEX_STEREO_CFG(_sample_rate)                                                         \
    {                                                                                                   \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                            \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                   \
    }

void Create_Std_I2S_Cfg(i2s_std_config_t *i2s_std_config_out)
{

    i2s_std_config_out->clk_cfg.sample_rate_hz = 16000;
    i2s_std_config_out->clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    i2s_std_config_out->clk_cfg.ext_clk_freq_hz = 0;
    i2s_std_config_out->clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    i2s_std_config_out->clk_cfg.bclk_div = 8;

    i2s_std_config_out->gpio_cfg.mclk = BSP_I2S_MCLK;
    i2s_std_config_out->gpio_cfg.bclk = BSP_I2S_SCLK;
    i2s_std_config_out->gpio_cfg.ws = BSP_I2S_LCLK;
    i2s_std_config_out->gpio_cfg.dout = BSP_I2S_DOUT;
    i2s_std_config_out->gpio_cfg.din = BSP_I2S_DSIN;
    i2s_std_config_out->gpio_cfg.invert_flags.mclk_inv = false;
    i2s_std_config_out->gpio_cfg.invert_flags.bclk_inv = false;
    i2s_std_config_out->gpio_cfg.invert_flags.ws_inv = false;

    i2s_std_config_out->slot_cfg.data_bit_width = 16;
    i2s_std_config_out->slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    i2s_std_config_out->slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    i2s_std_config_out->slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
    i2s_std_config_out->slot_cfg.ws_width = 16;
    i2s_std_config_out->slot_cfg.ws_pol = false;
    i2s_std_config_out->slot_cfg.bit_shift = true;
    i2s_std_config_out->slot_cfg.left_align = true;
    i2s_std_config_out->slot_cfg.big_endian = false;
    i2s_std_config_out->slot_cfg.bit_order_lsb = false;
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    esp_err_t ret = ESP_FAIL;
    if (tx_handle && rx_handle)
    {
        return ESP_OK;
    }

    /* Setup I2S peripheral */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    BSP_ERROR_CHECK_RETURN_ERR(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));

    /* Setup I2S channels */
    // marker BSP_I2S_DUPLEX_MONO_CFG(16000);
    // const i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_STEREO_CFG(16000);
    // const i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(16000);

    i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(16000);
    Create_Std_I2S_Cfg(&std_cfg_default);

    const i2s_std_config_t *p_i2s_cfg = &std_cfg_default;
    if (i2s_config != NULL)
    {
        p_i2s_cfg = i2s_config;
    }

    if (tx_handle != NULL)
    {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(tx_handle, p_i2s_cfg), err, TAG, "I2S channel initialization failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(tx_handle), err, TAG, "I2S enabling failed");
    }
    if (rx_handle != NULL)
    {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(rx_handle, p_i2s_cfg), err, TAG, "I2S channel initialization failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(rx_handle), err, TAG, "I2S enabling failed");
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = BSP_I2S_NUM,
        .rx_handle = rx_handle,
        .tx_handle = tx_handle,
    };
    i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    // BSP_NULL_CHECK_GOTO(i2s_data_if, err);

    return ESP_OK;

err:
    if (tx_handle)
    {
        i2s_del_channel(tx_handle);
    }
    if (rx_handle)
    {
        i2s_del_channel(rx_handle);
    }

    return ret;
}

const audio_codec_data_if_t *bsp_audio_get_codec_itf(void)
{
    return i2s_data_if;
}

// static esp_err_t bsp_i2s_init(i2s_port_t i2s_num, uint32_t sample_rate, i2s_channel_fmt_t channel_format, i2s_bits_per_chan_t bits_per_chan)
// esp_err_t bsp_i2s_init(int i2s_num, uint32_t sample_rate, int channel_format, int bits_per_chan)
// {
//   esp_err_t ret_val = ESP_OK;

//   i2s_slot_mode_t channel_fmt = I2S_SLOT_MODE_STEREO;
//   if (channel_format == 1)
//   {
//     channel_fmt = I2S_SLOT_MODE_MONO;
//   }
//   else if (channel_format == 2)
//   {
//     channel_fmt = I2S_SLOT_MODE_STEREO;
//   }
//   else
//   {
//     ESP_LOGE(TAG, "Unable to configure channel_format %d", channel_format);
//     channel_format = 1;
//     channel_fmt = I2S_SLOT_MODE_MONO;
//   }

//   if (bits_per_chan != 16 && bits_per_chan != 32)
//   {
//     ESP_LOGE(TAG, "Unable to configure bits_per_chan %d", bits_per_chan);
//     bits_per_chan = 32;
//   }

//   i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, I2S_ROLE_MASTER);
//   ret_val |= i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
//   i2s_std_config_t std_cfg = I2S_CONFIG_DEFAULT(sample_rate, channel_fmt, bits_per_chan);
//   ret_val |= i2s_channel_init_std_mode(tx_handle, &std_cfg);
//   ret_val |= i2s_channel_init_std_mode(rx_handle, &std_cfg);
//   ret_val |= i2s_channel_enable(tx_handle);
//   ret_val |= i2s_channel_enable(rx_handle);

//   return ret_val;
// }

esp_err_t bsp_i2s_deinit(int i2s_num)
{
    esp_err_t ret_val = ESP_OK;

    if (i2s_num == I2S_NUM_1 && tx_handle)
    {
        ret_val |= i2s_channel_disable(rx_handle);
        ret_val |= i2s_del_channel(rx_handle);
        rx_handle = NULL;
    }
    else if (i2s_num == I2S_NUM_0 && tx_handle)
    {
        ret_val |= i2s_channel_disable(tx_handle);
        ret_val |= i2s_del_channel(tx_handle);
        tx_handle = NULL;
    }

    return ret_val;
}

esp_err_t bsp_codec_init(int adc_sample_rate, int dac_sample_rate, int dac_channel_format, int dac_bits_per_chan)
{
    esp_err_t ret_val = ESP_OK;

    ret_val |= bsp_codec_adc_init(adc_sample_rate, dac_channel_format, dac_bits_per_chan);
    ret_val |= bsp_codec_dac_init(dac_sample_rate, dac_channel_format, dac_bits_per_chan);

    return ret_val;
}

esp_err_t bsp_codec_deinit()
{
    esp_err_t ret_val = ESP_OK;

    ret_val |= bsp_codec_adc_deinit();
    ret_val |= bsp_codec_dac_deinit();
    return ret_val;
}

// esp_err_t esp_board_init_28ai(uint32_t sample_rate, int channel_format, int bits_per_chan)
// {
//   /*!< Initialize I2C bus, used for audio codec*/

//    bsp_i2c_init();
//   // s_play_sample_rate = sample_rate;

//   if (channel_format != 2 && channel_format != 1)
//   {
//     ESP_LOGE(TAG, "Unable to configure channel_format");
//     channel_format = 2;
//   }
//   // s_play_channel_format = channel_format;

//   if (bits_per_chan != 32 && bits_per_chan != 16)
//   {
//     ESP_LOGE(TAG, "Unable to configure bits_per_chan");
//     bits_per_chan = 32;
//   }
//   // s_bits_per_chan = bits_per_chan;

//   // bsp_i2s_init(I2S_NUM_1, sample_rate, 2, 32);
//   bsp_i2s_init(I2S_NUM_1, sample_rate, channel_format, bits_per_chan);
//   // Because record and play use the same i2s.
//   // bsp_codec_init(sample_rate, sample_rate, 2, 32);
//   bsp_codec_init(sample_rate, sample_rate, channel_format, bits_per_chan);

//   /* Initialize PA */
//   /*gpio_config_t  io_conf;
//   memset(&io_conf, 0, sizeof(io_conf));
//   io_conf.intr_type = GPIO_INTR_DISABLE;
//   io_conf.mode = GPIO_MODE_OUTPUT;
//   io_conf.pin_bit_mask = ((1ULL << GPIO_PWR_CTRL));
//   io_conf.pull_down_en = 0;
//   io_conf.pull_up_en = 0;
//   gpio_config(&io_conf);
//   gpio_set_level(GPIO_PWR_CTRL, 1);*/

//   return ESP_OK;
// }

esp_codec_dev_handle_t esp_ret_play_dev(void)
{
    return play_dev;
}

i2c_master_bus_handle_t esp_ret_i2c_handle(void)
{
    return i2c_handle;
}

esp_err_t bsp_codec_adc_init(int sample_rate, int adc_channel_format, int adc_bits_per_chan)
{
    //   esp_err_t ret_val = ESP_OK;

    //   // Do initialize of related interface: data_if, ctrl_if and gpio_if
    //   audio_codec_i2s_cfg_t i2s_cfg = {
    //       .port = I2S_NUM_1,
    //       .rx_handle = rx_handle,
    //       .tx_handle = NULL,

    //   };
    //   record_data_if = audio_codec_new_i2s_data(&i2s_cfg);

    //   audio_codec_i2c_cfg_t i2c_cfg = {.addr = ES7210_CODEC_DEFAULT_ADDR, .bus_handle = i2c_handle};
    //   record_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    //   // New input codec interface
    //   es7210_codec_cfg_t es7210_cfg = {
    //       .ctrl_if = record_ctrl_if,
    //       .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2,

    //   };
    //   record_codec_if = es7210_codec_new(&es7210_cfg);
    //   // New input codec device
    //   esp_codec_dev_cfg_t dev_cfg = {
    //       .codec_if = record_codec_if,
    //       .data_if = record_data_if,
    //       .dev_type = ESP_CODEC_DEV_TYPE_IN,
    //   };
    //   record_dev = esp_codec_dev_new(&dev_cfg);

    //   esp_codec_dev_sample_info_t fs = {
    //       .sample_rate = sample_rate,
    //       .channel = adc_channel_format,
    //       .bits_per_sample = adc_bits_per_chan,
    //   };
    //   esp_codec_dev_open(record_dev, &fs);
    //   // esp_codec_dev_set_in_gain(record_dev, RECORD_VOLUME);
    //   esp_codec_dev_set_in_channel_gain(record_dev, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), RECORD_VOLUME);
    //   esp_codec_dev_set_in_channel_gain(record_dev, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1),RECORD_VOLUME);
    //   esp_codec_dev_set_in_channel_gain(record_dev, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2), 0);
    //   esp_codec_dev_set_in_channel_gain(record_dev, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(3), 0);

    return -1;
}

esp_err_t bsp_codec_dac_init(int sample_rate, int channel_format, int bits_per_chan)
{
    esp_err_t ret_val = ESP_OK;

    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {

        .port = BSP_I2C_NUM,
        .rx_handle = NULL,
        .tx_handle = tx_handle,

    };
    play_data_if = (audio_codec_data_if_t *)audio_codec_new_i2s_data(&i2s_cfg);

    audio_codec_i2c_cfg_t i2c_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR, .bus_handle = i2c_handle};
    play_ctrl_if = (audio_codec_ctrl_if_t *)audio_codec_new_i2c_ctrl(&i2c_cfg);
    play_gpio_if = (audio_codec_gpio_if_t *)audio_codec_new_gpio();
    // New output codec interface
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = play_ctrl_if,
        .gpio_if = play_gpio_if,
        .pa_pin = BSP_POWER_AMP_IO,
        .use_mclk = false,
    };
    play_codec_if = (audio_codec_if_t *)es8311_codec_new(&es8311_cfg);
    // New output codec device
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = play_codec_if,
        .data_if = play_data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    play_dev = esp_codec_dev_new(&dev_cfg);

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = bits_per_chan,
        .sample_rate = sample_rate,
        .channel = channel_format,
    };
    esp_codec_dev_set_out_vol(play_dev, BSP_PLAYER_VOLUME);
    esp_codec_dev_open(play_dev, &fs);

    return ret_val;
}

esp_err_t bsp_codec_adc_deinit()
{
    esp_err_t ret_val = ESP_OK;

    if (record_dev)
    {
        esp_codec_dev_close(record_dev);
        esp_codec_dev_delete(record_dev);
        record_dev = NULL;
    }

    // Delete codec interface
    if (record_codec_if)
    {
        audio_codec_delete_codec_if(record_codec_if);
        record_codec_if = NULL;
    }

    // Delete codec control interface
    if (record_ctrl_if)
    {
        audio_codec_delete_ctrl_if(record_ctrl_if);
        record_ctrl_if = NULL;
    }

    // Delete codec data interface
    if (record_data_if)
    {
        audio_codec_delete_data_if(record_data_if);
        record_data_if = NULL;
    }

    return ret_val;
}

esp_err_t bsp_codec_dac_deinit()
{
    esp_err_t ret_val = ESP_OK;

    if (play_dev)
    {
        esp_codec_dev_close(play_dev);
        esp_codec_dev_delete(play_dev);
        play_dev = NULL;
    }

    // Delete codec interface
    if (play_codec_if)
    {
        audio_codec_delete_codec_if(play_codec_if);
        play_codec_if = NULL;
    }

    // Delete codec control interface
    if (play_ctrl_if)
    {
        audio_codec_delete_ctrl_if(play_ctrl_if);
        play_ctrl_if = NULL;
    }

    if (play_gpio_if)
    {
        audio_codec_delete_gpio_if(play_gpio_if);
        play_gpio_if = NULL;
    }

    // Delete codec data interface
    if (play_data_if)
    {
        audio_codec_delete_data_if(play_data_if);
        play_data_if = NULL;
    }

    return ret_val;
}

esp_err_t esp_audio_set_play_vol(int volume)
{
    if (!play_dev)
    {
        ESP_LOGE(TAG, "DAC codec init fail");
        return ESP_FAIL;
    }
    esp_codec_dev_set_out_vol(play_dev, volume);
    return ESP_OK;
}

esp_err_t esp_audio_get_play_vol(int *volume)
{
    if (!play_dev)
    {
        ESP_LOGE(TAG, "DAC codec init fail");
        return ESP_FAIL;
    }
    esp_codec_dev_get_out_vol(play_dev, volume);
    return ESP_OK;
}

// static esp_err_t bsp_i2s_init(i2s_port_t i2s_num, uint32_t sample_rate, i2s_channel_fmt_t channel_format, i2s_bits_per_chan_t bits_per_chan)
esp_err_t bsp_i2s_init(int i2s_num, uint32_t sample_rate, int channel_format, int bits_per_chan)
{
    esp_err_t ret_val = ESP_OK;

    i2s_slot_mode_t channel_fmt = I2S_SLOT_MODE_STEREO;
    if (channel_format == 1)
    {
        channel_fmt = I2S_SLOT_MODE_MONO;
    }
    else if (channel_format == 2)
    {
        channel_fmt = I2S_SLOT_MODE_STEREO;
    }
    else
    {
        ESP_LOGE(TAG, "Unable to configure channel_format %d", channel_format);
        channel_format = 1;
        channel_fmt = I2S_SLOT_MODE_MONO;
    }

    if (bits_per_chan != 16 && bits_per_chan != 32)
    {
        ESP_LOGE(TAG, "Unable to configure bits_per_chan %d", bits_per_chan);
        bits_per_chan = 32;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, I2S_ROLE_MASTER);
    ret_val |= i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
    i2s_std_config_t std_cfg = I2S_CONFIG_DEFAULT(sample_rate, channel_fmt, bits_per_chan);
    ret_val |= i2s_channel_init_std_mode(tx_handle, &std_cfg);
    ret_val |= i2s_channel_init_std_mode(rx_handle, &std_cfg);
    ret_val |= i2s_channel_enable(tx_handle);
    ret_val |= i2s_channel_enable(rx_handle);

    return ret_val;
}

// esp_err_t bsp_i2s_deinit(int i2s_num)
// {
//   esp_err_t ret_val = ESP_OK;

// #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
//   if (i2s_num == I2S_NUM_1 && rx_handle)
//   {
//     ret_val |= i2s_channel_disable(rx_handle);
//     ret_val |= i2s_del_channel(rx_handle);
//     rx_handle = NULL;
//   }
//   else if (i2s_num == I2S_NUM_0 && tx_handle)
//   {
//     ret_val |= i2s_channel_disable(tx_handle);
//     ret_val |= i2s_del_channel(tx_handle);
//     tx_handle = NULL;
//   }
// #else
//   ret_val |= i2s_stop(i2s_num);
//   ret_val |= i2s_driver_uninstall(i2s_num);
// #endif

//   return ret_val;
// }

// esp_err_t bsp_codec_init(int adc_sample_rate, int dac_sample_rate, int dac_channel_format, int dac_bits_per_chan)
// {
//   esp_err_t ret_val = ESP_OK;

//   ret_val |= bsp_codec_adc_init(adc_sample_rate, dac_channel_format, dac_bits_per_chan);
//   ret_val |= bsp_codec_dac_init( dac_sample_rate , dac_channel_format , dac_bits_per_chan );

//   return ret_val;
// }

// esp_err_t bsp_codec_deinit()
// {
//   esp_err_t ret_val = ESP_OK;

//   ret_val |= bsp_codec_adc_deinit();
//   ret_val |= bsp_codec_dac_deinit();
//   return ret_val;
// }

// esp_err_t esp_audio_play(const int16_t *data, int length, uint32_t ticks_to_wait)
// {
//   size_t bytes_write = 0;
//   esp_err_t ret = ESP_OK;
//   if (!play_dev)
//   {
//     return ESP_FAIL;
//   }

//   int out_length = length;
//   int audio_time = 1;
//   audio_time *= (16000 / s_play_sample_rate);
//   audio_time *= (2 / s_play_channel_format);

//   int *data_out = NULL;
//   if (s_bits_per_chan != 32)
//   {
//     out_length = length * 2;
//     data_out = malloc(out_length);
//     for (int i = 0; i < length / sizeof(int16_t); i++)
//     {
//       int ret = data[i];
//       data_out[i] = ret << 16;
//     }
//   }

//   int *data_out_1 = NULL;
//   if (s_play_channel_format != 2 || s_play_sample_rate != 16000)
//   {
//     out_length *= audio_time;
//     data_out_1 = malloc(out_length);
//     int *tmp_data = NULL;
//     if (data_out != NULL)
//     {
//       tmp_data = data_out;
//     }
//     else
//     {
//       tmp_data = (int *)data;
//     }

//     for (int i = 0; i < out_length / (audio_time * sizeof(int)); i++)
//     {
//       for (int j = 0; j < audio_time; j++)
//       {
//         data_out_1[audio_time * i + j] = tmp_data[i];
//       }
//     }
//     if (data_out != NULL)
//     {
//       free(data_out);
//       data_out = NULL;
//     }
//   }

//   if (data_out != NULL)
//   {
//     ret = esp_codec_dev_write(play_dev, (void *)data_out, out_length);
//     free(data_out);
//   }
//   else if (data_out_1 != NULL)
//   {
//     ret = esp_codec_dev_write(play_dev, (void *)data_out_1, out_length);
//     free(data_out_1);
//   }
//   else
//   {
//     ret = esp_codec_dev_write(play_dev, (void *)data, length);
//   }

//   return ret;
// }

// esp_err_t esp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len)
// {
//   esp_err_t ret = ESP_OK;
//   size_t bytes_read;
//   int audio_chunksize = buffer_len / (sizeof(int16_t) * ADC_I2S_CHANNEL);

//   ret = esp_codec_dev_read(record_dev, (void *)buffer, buffer_len);
//   if (!is_get_raw_channel)
//   {
//     for (int i = 0; i < audio_chunksize; i++)
//     {
//       int16_t ref = buffer[4 * i + 0];
//       buffer[3 * i + 0] = buffer[4 * i + 1];
//       buffer[3 * i + 1] = buffer[4 * i + 3];
//       buffer[3 * i + 2] = ref;
//     }
//   }

//   return ret;
// }

esp_err_t esp_board_init(uint32_t sample_rate, int channel_format, int bits_per_chan)
{
    /*!< Initialize I2C bus, used for audio codec*/

    bsp_i2c_init();
    // s_play_sample_rate = sample_rate;

    if (channel_format != 2 && channel_format != 1)
    {
        ESP_LOGE(TAG, "Unable to configure channel_format");
        channel_format = 2;
    }
    // s_play_channel_format = channel_format;

    if (bits_per_chan != 32 && bits_per_chan != 16)
    {
        ESP_LOGE(TAG, "Unable to configure bits_per_chan");
        bits_per_chan = 32;
    }
    // s_bits_per_chan = bits_per_chan;

    // bsp_i2s_init(I2S_NUM_1, sample_rate, 2, 32);
    bsp_i2s_init(I2S_NUM_1, sample_rate, channel_format, bits_per_chan);
    // Because record and play use the same i2s.
    // bsp_codec_init(sample_rate, sample_rate, 2, 32);
    bsp_codec_init(sample_rate, sample_rate, channel_format, bits_per_chan);

    /* Initialize PA */
    /*gpio_config_t  io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((1ULL << GPIO_PWR_CTRL));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_PWR_CTRL, 1);*/

    return ESP_OK;
}
