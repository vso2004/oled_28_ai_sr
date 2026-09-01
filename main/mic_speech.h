#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#ifdef __cplusplus
extern "C" {
#endif



typedef enum {
    ESP_SR_EVT_AWAKEN,  
    ESP_SR_EVT_CMD,
    ESP_SR_EVT_CMD_TIMEOUT
} esp_sr_rec_event_t;


typedef union { 
    uint8_t awaken_channel;    // 唤醒通道
    uint8_t sr_cmd;             // 识别命令ID
} esp_sr_evt_data_t;



typedef void (*esp_sr_event_callback_t)(esp_sr_rec_event_t event,esp_sr_evt_data_t evt_data, void *user_data);



void Speech_Init(void);
esp_err_t Speech_register_callback(esp_sr_event_callback_t callback);
void feed_Task( void* arg );
void detect_Task( void* arg );
void Speech_Init( void );
esp_err_t Speech_register_callback( esp_sr_event_callback_t callback );
char *esp_mn_commands_get_string(int command_id);

#ifdef __cplusplus
}
#endif
