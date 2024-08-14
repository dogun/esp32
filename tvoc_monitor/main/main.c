#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <freertos/event_groups.h>
#include <sys/param.h>
#include <freertos/semphr.h>
#include <driver/touch_pad.h>
#include "config.h"
#include "config_store.h"
#include "http_op.h"
#include "wifi.h"
#include "http_c.h"
#include "tvoc.h"

#define TAG "MAIN"

void app_main(void) {
	init_wifi();

	tvoc_run();

	ESP_ERROR_CHECK(touch_pad_init());
	touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
	touch_pad_config(TOUCH_PAD_NUM8, 0);

    uint16_t touch_value;
    int touch_times = 0;
    
    int _tvoc = 0;
    int _co2 = 0;
    int _jq = 0;
    
    while (true) {
    	touch_pad_read(TOUCH_PAD_NUM8, &touch_value);
    	if (touch_value < 500) {
    		ESP_LOGI(TAG, "touch value: %d", touch_value);
    		touch_times ++;
    	} else {
    		touch_times = 0;
    	}
    	if (touch_times > 4) {
    		reconfig_wifi();
    	}

    	run_wifi();
    	
   		if (wifi_ready == 1) {
			 if (_tvoc != tvoc || _co2 != co2 || _jq != jq) {
				 set_data(tvoc, co2, jq, sta_uid);
				 _tvoc = tvoc;
				 _co2 = co2;
				 _jq = jq;
			 }
		}

    	vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
