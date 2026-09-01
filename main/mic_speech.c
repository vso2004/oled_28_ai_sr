#include "mic_speech.h"

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "model_path.h"
#include "esp_process_sdkconfig.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

const char *TAG = "App/Speech";

int wakeup_flag = 0;
static volatile int task_flag = 0;
static esp_afe_sr_iface_t *afe_handle = NULL;
extern QueueHandle_t xQueue;

srmodel_list_t *models = NULL;
static esp_sr_event_callback_t spench_callback = NULL;
void esp_mn_active_commands_print(void);

// int16_t mic_data[BSP_RECV_BUF_SIZE];

// int16_t *i2s_buff = 0;
// int16_t i2s_buff[BSP_RECV_BUF_SIZE * 2] = {0};
int16_t *i2s_buff = 0;
// int16_t *pi2s_buff = &i2s_buff[0];

model_iface_data_t *model_data = 0;

// void audio_task(void *pvParameters)
// {

//   //esp_codec_dev_handle_t *codec =
//   bsp_audio_codec_speaker_init();
//   int16_t xxx[64];
//   size_t bytes_read = 0;
//   size_t bytes_write = 0;

//   i2s_buff = heap_caps_malloc(BSP_RECV_BUF_SIZE * sizeof(int16_t) * 2, MALLOC_CAP_INTERNAL);

//   models = esp_srmodel_init("model"); // partition label defined in partitions.csv
//                                       //"MR"
//   afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);

//   // AFE_MODE_HIGH_PERF
//   // afe_config_t* afe_config = afe_config_init( esp_get_input_format() , models , AFE_TYPE_SR , AFE_MODE_LOW_COST );
//   afe_config->ns_init = false;
//   afe_config->vad_init = false;
//   afe_handle = (esp_afe_sr_iface_t *)esp_afe_handle_from_config(afe_config);
//   esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
//   afe_config_print(afe_config);

//   afe_config_free(afe_config);

//   int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
//   int nch = afe_handle->get_feed_channel_num(afe_data);
//   int feed_channel = ADC_I2S_CHANNEL; // vso2004

//   printf("\naudio_chunksize:%d", audio_chunksize);
//   printf("feed_channel :%d", feed_channel);
//   printf(" nch:%d ", nch);
//   printf("feed_channel :%d\n", feed_channel);
//   assert(nch == feed_channel);

//    //printf("\n start loop\n");

//   while (1)
//   {
//     // esp_codec_dev_read(  i2s_rx_chan , ( void* )i2s_buff , BSP_RECV_BUF_SIZE);
//     i2s_channel_read(rx_handle, i2s_buff, BSP_RECV_BUF_SIZE, &bytes_read, 1000);
//     //memcpy(xxx,i2s_buff,64);
//     afe_handle->feed(afe_data, i2s_buff);
//     //for (int i = 0; i < BSP_RECV_BUF_SIZE / 2; i += 2)
//     //{
//     //  i2s_buff[i + 1] = i2s_buff[i];
//     //}
//     afe_fetch_result_t *res = afe_handle->fetch(afe_data);
//     //printf("\nres->data_size : %d\n",res->data_size);
//     i2s_channel_write(tx_handle, i2s_buff, BSP_RECV_BUF_SIZE, &bytes_write, 1000);
//     // esp_codec_dev_write(  i2s_tx_chan , ( void* )i2s_buff , BSP_RECV_BUF_SIZE);
//   }

//   esp_restart();
// }

void feed_Task(void *arg)
{

  size_t bytes_read = 0;
  // size_t bytes_write = 0;

  esp_afe_sr_data_t *afe_data = arg;
  int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
  int nch = afe_handle->get_feed_channel_num(afe_data);
  int feed_channel = esp_get_feed_channel(); // vso2004

  printf("\nfeed_Task audio_chunksize:%d", audio_chunksize);
  printf(" feed_channel :%d", feed_channel);
  printf(" nch:%d ", nch);
  printf(" feed_channel :%d\n", feed_channel);
  // vTaskDelay(pdMS_TO_TICKS(1000));

  assert(nch == feed_channel);

  int16_t *i2s_buff = heap_caps_malloc(audio_chunksize * sizeof(int16_t) * feed_channel, MALLOC_CAP_SPIRAM);

  esp_task_wdt_add(NULL);

  while (task_flag)
  {
    size_t ReadLen = audio_chunksize * sizeof(int16_t) * feed_channel;
    i2s_channel_read(rx_handle, i2s_buff, ReadLen, &bytes_read, 100);

    afe_handle->feed(afe_data, i2s_buff);

    esp_task_wdt_reset();
  }

  esp_restart();
  vTaskDelete(NULL);
}

void detect_Task(void *arg)
{
  esp_afe_sr_data_t *afe_data = arg;
  int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);

  char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
  // ESP_LOGI( "xxx" , "multinet:%s\n" , mn_name );
  // vTaskDelay( 1000 / portTICK_PERIOD_MS );

  esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
  model_data = multinet->create(mn_name, 6000);
  //multinet->set_det_threshold(model_data,0.7);
  esp_mn_commands_update_from_sdkconfig(multinet, model_data); // Add speech commands from sdkconfig
  int mu_chunksize = multinet->get_samp_chunksize(model_data);

  // printf("\n afe_chunksize:%d\n", afe_chunksize);
  // printf("\n fmu_chunksize :%d\n", mu_chunksize);
  assert(afe_chunksize == mu_chunksize);
  esp_mn_active_commands_print();

  // int16_t *buff = malloc(afe_chunksize * sizeof(int16_t));

  esp_task_wdt_add(NULL);
  wakeup_flag = 0;
  while (task_flag)
  {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1));
    afe_fetch_result_t *res = afe_handle->fetch(afe_data);

    if (!res || res->ret_value == ESP_FAIL)
    {
      printf("fetch error!\n");
      break;
    }

    if (res->wakeup_state == WAKENET_DETECTED)
    {
      // printf("WAKEWORD DETECTED\n");

      xQueueSend(xQueue, "WAKE", pdMS_TO_TICKS(100));
      multinet->clean(model_data);
      afe_handle->disable_wakenet(afe_data);
    }

    if (res->raw_data_channels == 1 && res->wakeup_state == WAKENET_DETECTED)
    {
      wakeup_flag = 1;
      // afe_handle->disable_wakenet(afe_data);
    }
    else if (res->raw_data_channels > 1 && res->wakeup_state == WAKENET_CHANNEL_VERIFIED)
    {
      // For a multi-channel AFE, it is necessary to wait for the channel to be verified.
     // printf("AFE_FETCH_CHANNEL_VERIFIED xxx, channel index: %d\n", res->trigger_channel_id);
      esp_sr_evt_data_t evtdata;
      evtdata.awaken_channel = res->trigger_channel_id;
      if (spench_callback != NULL)
      {
        spench_callback(ESP_SR_EVT_AWAKEN, evtdata, NULL);
      }
      wakeup_flag = 1;
      // afe_handle->disable_wakenet(afe_data);
    }

    if (wakeup_flag == 0)
    {

      continue;
    }

    esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

    if (mn_state == ESP_MN_STATE_DETECTING)
    {
      // ESP_LOGI("detect task", "multinet->detect len %d\n",res->data_size);
      continue;
    }

    if (mn_state == ESP_MN_STATE_DETECTED)
    {
      esp_mn_results_t *mn_result = multinet->get_results(model_data);

      // ESP_LOGI(TAG, "TOP xxx %d, command_id: %d, phrase_id: %d, string:%s prob: %f\n", 1, mn_result->command_id[0], mn_result->phrase_id[0], mn_result->string, mn_result->prob[0]);

      esp_sr_evt_data_t evtdata;
      evtdata.sr_cmd = mn_result->command_id[0];
      if (spench_callback != NULL)
      {
        char command_name[64];
        char *ptmp = esp_mn_commands_get_string(evtdata.sr_cmd);
        size_t cmd_name_tmp = strlen(ptmp);
        if (cmd_name_tmp > 0 && cmd_name_tmp < 64)
          strcpy(command_name, ptmp);
        else
          strcpy(command_name, "no command name");

        printf("command string:%s\n", command_name);


        spench_callback(ESP_SR_EVT_CMD, evtdata, NULL);
        mn_state = ESP_MN_STATE_TIMEOUT;
        afe_handle->reset_buffer(afe_data);
        afe_handle->enable_wakenet(afe_data);
        multinet->clean(model_data);
        char tmp[128];
        sprintf(tmp, "CMD:%s", command_name);
        xQueueSend(xQueue, tmp, pdMS_TO_TICKS(100));
      }
      wakeup_flag = 0;

      continue;
    }

    if (mn_state == ESP_MN_STATE_TIMEOUT)
    {
      // esp_mn_results_t *mn_result = multinet->get_results(model_data);
      //  printf("timeout, string:%s\n", mn_result->string);
      xQueueSend(xQueue, "TIMEOUT", pdMS_TO_TICKS(100));
      // esp_sr_evt_data_t evtdata;
      // if (spench_callback != NULL)
      // {
      //   spench_callback(ESP_SR_EVT_CMD, evtdata, NULL);
      //   mn_state = ESP_MN_STATE_TIMEOUT;
      // }
      wakeup_flag = 0;
      //afe_handle->disable_wakenet(afe_data);
      afe_handle->reset_buffer(afe_data);
      afe_handle->enable_wakenet(afe_data);
      //multinet->clean(model_data);

      continue;
    }
  }

  // if (model_data)
  // {
  //   multinet->destroy(model_data);
  //   model_data = NULL;
  // }
  // printf("detect exit\n");
  // vTaskDelete(NULL);
}

void Speech_Init(void)
{
  bsp_audio_codec_speaker_init();

  models = esp_srmodel_init("model"); // partition label defined in partitions.csv
                                      //"MR"
  afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);

  // AFE_MODE_HIGH_PERF
  // afe_config_t* afe_config = afe_config_init( esp_get_input_format() , models , AFE_TYPE_SR , AFE_MODE_LOW_COST );
  afe_config->ns_init = false;
  afe_config->vad_init = false;
  afe_handle = (esp_afe_sr_iface_t *)esp_afe_handle_from_config(afe_config);
  esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
  afe_config_print(afe_config);
  afe_config_free(afe_config);

  // int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
  int nch = afe_handle->get_feed_channel_num(afe_data);
  int feed_channel = ADC_I2S_CHANNEL; // vso2004

  printf("\n nch:%d\n", nch);
  printf("\n feed_channel :%d\n", feed_channel);
  assert(nch == feed_channel);

  task_flag = 1;
  xTaskCreatePinnedToCore(&detect_Task, "detect", 8 * 1024, (void *)afe_data, 5, NULL, 0);
  xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void *)afe_data, 5, NULL, 1);
}

esp_err_t Speech_register_callback(esp_sr_event_callback_t callback)
{
  if (!callback)
    return ESP_ERR_INVALID_ARG;
  spench_callback = callback;
  return ESP_OK;
}