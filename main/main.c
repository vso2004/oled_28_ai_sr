#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "mic_speech.h"
#include "rgb_led_driver.h"

// const char *TAG = "app_main";

#define LOG_MEM_INFO (0)
QueueHandle_t xQueue;

void I2S_Loop(void);
void Speech_Init(void);
void Set_RGB_led(uint8_t programm);
void RGB_init();
// lv_obj_t *lv_obj_find_by_name(const lv_obj_t *parent, const char *name);

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_log.h"



// typedef struct
// {
//     uint8_t payload[256];
// } MyDataStruct;

// #define QUEUE_LENGTH 16
// #define ITEM_SIZE sizeof(MyDataStruct) // Replace with your data size

// QueueHandle_t create_psram_queue(void)
// {
//     // 1. Allocate the queue storage buffer in PSRAM
//     size_t buffer_size = (size_t)(QUEUE_LENGTH * ITEM_SIZE);
//     uint8_t *puiQueueStorage = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);

//     if (puiQueueStorage == NULL)
//     {
//         ESP_LOGE("QUEUE", "Failed to allocate queue storage in PSRAM!");
//         return NULL;
//     }

//     // 2. Allocate the Queue Control Structure (Can be internal RAM or PSRAM)
//     // Note: Placing the control structure itself in internal RAM ensures faster performance
//     StaticQueue_t *pxQueueBuffer = (StaticQueue_t *)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_INTERNAL);

//     if (pxQueueBuffer == NULL)
//     {
//         ESP_LOGE("QUEUE", "Failed to allocate queue control block!");
//         heap_caps_free(puiQueueStorage);
//         return NULL;
//     }

//     // 3. Create the static queue
//     QueueHandle_t xQueue = xQueueCreateStatic(
//         QUEUE_LENGTH,
//         ITEM_SIZE,
//         puiQueueStorage,
//         pxQueueBuffer);

//     return xQueue;
// }


// void Generate_Tone(int16_t *out)
// {
// }

static void button_matrix_event_cb(lv_event_t *e)
{
    lv_obj_t *bm = lv_event_get_target_obj(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(bm);
    // const char *text = lv_buttonmatrix_get_button_text(bm, id);
    char buf[16];
    sprintf(buf, "BTN%d", (uint8_t)(id & 0xff));
    xQueueSend(xQueue, buf, pdMS_TO_TICKS(100));
    lv_obj_t *status_label = lv_obj_find_by_name(lv_screen_active(), "status_label");

    // LV_LOG_USER("buttonmatrix: pressed %u (\"%s\")", (unsigned)id, text ? text : "");
    // printf("buttonmatrix: pressed %u (\"%s\")", (unsigned)id, text ? text : "");
    lv_label_set_text(status_label, buf);
}

void lv_example_buttonmatrix(void)
{
    static const char *map[] = {LV_SYMBOL_UP, LV_SYMBOL_DOWN, "\n", LV_SYMBOL_PLUS, LV_SYMBOL_MINUS, "\n", LV_SYMBOL_CALL, LV_SYMBOL_COPY, NULL};

    // lv_obj_t *scr = lv_screen_active();

    static lv_style_t style_item;
    static lv_style_t style_item_checked;
    // static lv_style_t style_btnm_font;

    // lv_style_init(&style_btnm_font);

    /* 2. Set the font size (e.g., Montserrat 20) */
    // lv_style_set_text_font(&style_btnm_font, &lv_font_montserrat_18);

    /* 3. Apply the style to the button matrix */

    lv_style_init(&style_item);
    lv_style_set_text_font(&style_item, &lv_font_montserrat_46);
    lv_style_set_bg_color(&style_item, lv_color_hex(0x0FFFF6));
    lv_style_set_radius(&style_item, 50);
    lv_style_set_text_color(&style_item, lv_color_hex(0x111827));

    lv_style_init(&style_item_checked);
    lv_style_set_bg_color(&style_item_checked, lv_color_hex(0x00ff00));
    lv_style_set_text_color(&style_item_checked, lv_color_hex(0xffffff));

    /* 2. Create button matrix */
    lv_obj_t *btnm = lv_buttonmatrix_create(lv_screen_active());

    /* 3. Apply styles to specific parts */
    lv_obj_add_style(btnm, &style_item, LV_PART_ITEMS);
    lv_obj_add_style(btnm, &style_item_checked, LV_PART_ITEMS | LV_STATE_PRESSED);
    // lv_obj_add_style(btnm, &style_btnm_font, LV_PART_ITEMS);

    /* 4. Adjust gap spacing between virtual buttons */
    lv_obj_set_style_pad_row(btnm, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(btnm, 6, LV_PART_MAIN);

    // lv_obj_set_style_flex_cross_place(scr, LV_FLEX_ALIGN_CENTER, 0);
    // lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
    // lv_obj_t *btnm = lv_buttonmatrix_create(scr);

    lv_obj_set_align(btnm, LV_ALIGN_CENTER);
    lv_obj_set_size(btnm, LV_PCT(100), LV_PCT(90));

    // lv_display_set_physical_resolution(scr,480, 480);
    // lv_obj_set_size(btnm, LV_PCT(100), LV_PCT(150));
    // lv_obj_set_flex_flow(btnm, LV_FLEX_FLOW_COLUMN | LV_FLEX_FLOW_ROW);

    // lv_obj_set_style_pad_row(btnm, 12, 0);

    lv_buttonmatrix_set_map(btnm, map);
    lv_obj_add_event_cb(btnm, button_matrix_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *status_label = lv_label_create(lv_screen_active());
    lv_obj_set_name(status_label, "status_label");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(status_label, "Pressed: —");
}

static void Speech_event_callback(esp_sr_rec_event_t event, esp_sr_evt_data_t evt_data, void *user_data)
{
    ESP_LOGI("Speech_event_callback", "ESP_SR_EVT_AWAKEN = %d cmd %d ", evt_data.awaken_channel, evt_data.sr_cmd);

    switch (event)
    {
    case ESP_SR_EVT_AWAKEN:
    {
        // ESP_LOGI("Speech_event_callback", "ESP_SR_EVT_AWAKEN = %d cmd %d ", evt_data.awaken_channel, evt_data.sr_cmd);
        return;
    }
    case ESP_SR_EVT_CMD:
    {
        ESP_LOGI("Speech_event_callback", "ESP_SR_EVT_CMD = %d", evt_data.sr_cmd);
        Set_RGB_led(evt_data.sr_cmd);
        return;
    }
    default:
        break;
    }
}

void Loop_Task(void *arg)
{
    esp_task_wdt_add(NULL);
    while (1)
    {
        // printf(".\n");
        char req_buf[16];

        BaseType_t xStatus = xQueueReceive(xQueue, req_buf, 100);

        if (xStatus != pdPASS)
        {
            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        esp_task_wdt_reset();
        printf("read_state:%s\n", req_buf);
        int cmp_res = -1;

        cmp_res = strcmp(req_buf, "WAIT");
        if (cmp_res == 0)
        {
            lv_obj_t *status_label = lv_obj_find_by_name(lv_screen_active(), "status_label");
            lv_label_set_text(status_label, "WAIT");
            continue;
        }

        if (req_buf[0] == 'C' && req_buf[1] == 'M' && req_buf[2] == 'D')
        {
            lv_obj_t *status_label = lv_obj_find_by_name(lv_screen_active(), "status_label");
            lv_label_set_text(status_label, req_buf);
            vTaskDelay(pdMS_TO_TICKS(1200));
            lv_label_set_text(status_label, "WAIT");
            continue;
        }
        cmp_res = strcmp(req_buf, "TIMEOUT");
        if (cmp_res == 0)
        {
            lv_obj_t *status_label = lv_obj_find_by_name(lv_screen_active(), "status_label");
            lv_label_set_text(status_label, "TIMEOUT");
            continue;
        }

        cmp_res = strcmp(req_buf, "BTN");
        if (cmp_res == 0)
        {
            printf("BTN\n");
        }

        cmp_res = strcmp(req_buf, "WAKE");
        if (cmp_res == 0)
        {
            lv_obj_t *status_label = lv_obj_find_by_name(lv_screen_active(), "status_label");
            lv_label_set_text(status_label, "WAKE");
        }
    }
}

// MARKER
void app_main(void)
{
    xQueue = xQueueCreate(6, 128);

    RGB_init();
    bsp_i2c_init();
    bsp_display_brightness_set(100);
    bsp_display_start();

    // esp_task_wdt_config_t twdt_config = {
    //     .timeout_ms = 10000,
    //     .idle_core_mask = (1 << configNUM_CORES) - 1,
    //     //.panic_on_timeout = true,
    // };
    // esp_task_wdt_init(&twdt_config);

    ESP_LOGI("app_main", "Display LVGL demo sr");
    bsp_display_lock(0);
    lv_example_buttonmatrix();
    bsp_display_unlock();

    Speech_Init();
    Speech_register_callback(Speech_event_callback);

    xTaskCreatePinnedToCore(&Loop_Task, "loop_Task", 4 * 1024, NULL, 5, NULL, 1);
    // esp_task_wdt_add(NULL);

    return;
}
