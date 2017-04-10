#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "mg_task.h"
#include "spiffs_vfs.h"


DRAM_ATTR const char main_tag[] = "main";

esp_err_t event_handler(void *ctx, system_event_t *event)
{
  if(event->event_id == SYSTEM_EVENT_STA_GOT_IP) {
    ESP_LOGD(main_tag, "Got an IP: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
    xTaskCreatePinnedToCore(&mg_task, "mongoose", 20000, NULL, 5, NULL, 0);
  }

  if (event->event_id == SYSTEM_EVENT_STA_START) {
    ESP_LOGD(main_tag, "Received a start request");
    ESP_ERROR_CHECK(esp_wifi_connect());
  }

  return ESP_OK;
}

void app_main(void)
{
  nvs_flash_init();
  tcpip_adapter_init();
  ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  wifi_config_t sta_config = {
    .sta = {
      .ssid = SSID,
      .password = PASS,
      .bssid_set = false
    }
  };
  ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
  ESP_ERROR_CHECK( esp_wifi_start() );
  ESP_ERROR_CHECK( esp_wifi_connect() );

  const esp_partition_t *p = esp_partition_find_first(0x40, 0x0, NULL);
  ESP_LOGI(main_tag, ">> Parition %s address: %x, size: %x", p->label, p->address, p->size);
  spiffs_register_vfs("/data", p);
}
