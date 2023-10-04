#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "led_i2c.h"
#include "http_op.h"
#include "wifi.h"
#include "config_store.h"

static const char *TAG = "LED-Master";

#define WIFI_RESET GPIO_NUM_23

void app_main(void) {
	ESP_ERROR_CHECK(i2c_master_init());
	ESP_LOGI(TAG, "I2C initialized successfully");
	init_wifi();

	gpio_reset_pin(WIFI_RESET);
	gpio_set_direction(WIFI_RESET, GPIO_MODE_INPUT);

	int wifi_reset_cnt = 0;
    while (true) {
    	int gpio_level = gpio_get_level(WIFI_RESET);
    	if (gpio_level == 1) {
    		wifi_reset_cnt ++;
    	}
    	if (wifi_reset_cnt > 2) {
    		reconfig_wifi();
    		wifi_reset_cnt = 0;
    	}

    	run_wifi();

		uint8_t bf[10] = {0};
		read_from_board(0, bf, sizeof(bf));
		if (bf[0] > 0)
			ESP_LOGI(TAG, "read data: %s", bf);

    	vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

	ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));
	ESP_LOGI(TAG, "I2C de-initialized successfully");
}
