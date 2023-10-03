/*
 * pcnt.h
 *
 *  Created on: 2023��1��26��
 *      Author: yuexu
 */

#ifndef MAIN_PCNT_H_
#define MAIN_PCNT_H_

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

static const char *PCNT_TAG = "PCNT";

#define PCNT_HIGH_LIMIT 10000
#define PCNT_LOW_LIMIT  -10000

#define EC11_GPIO_A 22
#define EC11_GPIO_B -1

static pcnt_unit_handle_t pcnt_unit = NULL;
void init_pcnt() {
    ESP_LOGI(PCNT_TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit = PCNT_LOW_LIMIT,
    };

    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    ESP_LOGI(PCNT_TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 100,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    ESP_LOGI(PCNT_TAG, "install pcnt channels");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = EC11_GPIO_A,
        .level_gpio_num = EC11_GPIO_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    ESP_LOGI(PCNT_TAG, "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_HOLD, PCNT_CHANNEL_EDGE_ACTION_INCREASE));

    ESP_LOGI(PCNT_TAG, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_LOGI(PCNT_TAG, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_LOGI(PCNT_TAG, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

int get_pcnt() {
	int pulse_count;
	ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &pulse_count));
	//ESP_LOGI(PCNT_TAG, "Pulse count: %d", pulse_count);
	return pulse_count;
}

void clear_pcnt() {
	ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
}

#endif /* MAIN_PCNT_H_ */
