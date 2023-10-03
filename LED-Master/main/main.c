#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "led_i2c.h"
#include "http_op.h"
#include "led_wifi.h"
#include "config_store.h"

static const char *TAG = "LED-Master";

void app_main(void) {
	init_fs();
	ESP_LOGI(TAG, "nvs fs initialized successfully");
	ESP_ERROR_CHECK(i2c_master_init());
	ESP_LOGI(TAG, "I2C initialized successfully");
	wifi_init_softap();
	ESP_LOGI(TAG, "wifi initialized successfully");
	ESP_ERROR_CHECK(esp_wifi_start());
	httpd_handle_t server = start_webserver();
	ESP_LOGI(TAG, "webserver initialized successfully");
	while (1) {
		uint8_t bf[10] = {0};
		read_from_board(0, bf, sizeof(bf));
		if (bf[0] > 0)
			ESP_LOGI(TAG, "read data: %s", bf);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	stop_webserver(server);
	ESP_LOGI(TAG, "webserver de-initialized successfully");
	wifi_deinit_softap();
	ESP_LOGI(TAG, "wifi de-initialized successfully");
	ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));
	ESP_LOGI(TAG, "I2C de-initialized successfully");
}
