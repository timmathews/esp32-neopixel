#include <stdlib.h>
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
#include "neopixel.h"


pixel_settings_t px = NEOPIXEL_INIT_CONFIG_DEFAULT();
pixel_t pixel = {
	.red = 0,
	.green = 0,
	.blue = 0,
	.white = 3
};

uint8_t animation = 0;
SemaphoreHandle_t xSemaphore = NULL;

DRAM_ATTR const char main_tag[] = "main";

void chase() {
	pixel_t p;
	static uint8_t i = 1;

	if(xSemaphore != NULL && xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE) {
		p.red = pixel.red;
		p.green = pixel.green;
		p.blue = pixel.blue;
		p.white = pixel.white;
		xSemaphoreGive(xSemaphore);
	} else {
		ESP_LOGE(main_tag, "Could not get semaphore");
	}

	np_clear(&px);

	np_set_pixel_color(&px, i, p.red, p.green, p.blue, p.white);

	i++;

	if(i == px.pixel_count) i = 1;

	np_show(&px);

	vTaskDelay(200 / portTICK_PERIOD_MS);
}

void random() {
	np_clear(&px);

	if(xSemaphore != NULL && xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE) {
		np_set_pixel_color(&px, rand() % px.pixel_count, pixel.red, pixel.green, pixel.blue, pixel.white);
		xSemaphoreGive(xSemaphore);
	} else {
		ESP_LOGE(main_tag, "Could not get semaphore");
	}

	np_show(&px);
	vTaskDelay(200 / portTICK_PERIOD_MS);
}

void fade() {
	pixel_t p;
	if(xSemaphore != NULL && xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE) {
		p.red = pixel.red;
		p.green = pixel.green;
		p.blue = pixel.blue;
		p.white = pixel.white;
		xSemaphoreGive(xSemaphore);
	} else {
		ESP_LOGE(main_tag, "Could not get semaphore");
	}

	np_clear(&px);

	uint8_t i, r, g, b;
	float hsb[3];

	rgb_to_hsb(p.red, p.green, p.blue, hsb);

	ESP_LOGI(main_tag, "HSB: %f, %f, %f", hsb[0], hsb[1], hsb[2]);

	i = (uint8_t)(hsb[2] * 255.0);
	uint8_t j = 10;

	for(; j < i; ++j) {
		uint32_t c = hsb_to_rgb(hsb[0], hsb[1], (float)j / 255.0);
		r = c >> 16;
		g = (c >> 8) & 0xff;
		b = c & 0xff;
		np_set_pixel_color(&px, 0, r, g, b, 0);
		np_show(&px);
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}

	for(; j > 10; --j) {
		uint32_t c = hsb_to_rgb(hsb[0], hsb[1], (float)j / 255.0);
		r = c >> 16;
		g = (c >> 8) & 0xff;
		b = c & 0xff;
		np_set_pixel_color(&px, 0, r, g, b, 0);
		np_show(&px);
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

static void change_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t white, uint8_t ani) {
	if(xSemaphore != NULL && xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE) {
		pixel.red = red;
		pixel.green = green;
		pixel.blue = blue;
		pixel.white = white;
		animation = ani;
		xSemaphoreGive(xSemaphore);
	}
}

esp_err_t event_handler(void *ctx, system_event_t *event)
{
	if(event->event_id == SYSTEM_EVENT_STA_GOT_IP) {
		ESP_LOGD(main_tag, "Got an IP: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
		xTaskCreatePinnedToCore(&mg_os_task, "mongoose", 20000, change_color, 5, NULL, 0);
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

	px.items = malloc(sizeof(rmt_item32_t) * ((7 * 32) + 1));
	px.pixels = malloc(sizeof(pixel_t) * 7);
	px.pixel_count = 7;

	rmt_config_t rx = NEOPIXEL_RMT_INIT_CONFIG_DEFAULT(21, 0);
	ESP_ERROR_CHECK(rmt_config(&rx));
	ESP_ERROR_CHECK(rmt_driver_install(RMT_CHANNEL_0, 0, 0));

	srand(0x8191);
	vSemaphoreCreateBinary(xSemaphore);

	np_clear(&px);
	for(uint8_t i = 0; i < px.pixel_count; ++i) {
		np_set_pixel_color(&px, i, 0, 0, 0, 15);
	}
	np_show(&px);

	while(1) {
		switch(animation) {
			case 1:
				chase();
				break;
			case 2:
				fade();
				break;
			case 5:
				random();
				break;
			default:
				vTaskDelay(200 / portTICK_PERIOD_MS);
				break;
		}
	}
}
