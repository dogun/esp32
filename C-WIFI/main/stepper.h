/*
 * stepper.h
 *
 *  Created on: Jan 30, 2023
 *      Author: yuexu
 */

#ifndef MAIN_STEPPER_H_
#define MAIN_STEPPER_H_

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"

static const gpio_num_t step_pin = GPIO_NUM_12;
static const gpio_num_t direction_pin = GPIO_NUM_14;

static const int32_t speed_min = 32; // Hz.
static const int32_t speed_max = 19000; // Hz

static ledc_timer_config_t ledc_timer = {
        .timer_num = LEDC_TIMER_0,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .clk_cfg = LEDC_AUTO_CLK,
        .freq_hz = speed_min,
        .duty_resolution = LEDC_TIMER_5_BIT,
    };

static ledc_channel_config_t ledc_channel = {
        .timer_sel  = LEDC_TIMER_0,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0, // Start out stopped (0% duty cycle)
        .hpoint     = 0,
        .gpio_num   = step_pin,
    };

#define STEPPER_TAG "stepper"

void init_stepper(void) {
    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel);

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .pin_bit_mask = (1ULL<<direction_pin),
    };
    gpio_config(&io_conf);
}

void stepper(bool dir, int speed) {
	ESP_LOGI(STEPPER_TAG, "set stepper: %d", speed);
	gpio_set_level(direction_pin, dir);
	if (speed < speed_min || speed > speed_max) {
		ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
		ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
	} else {
		// 16 is 50% duty cycle in 5-bit PWM resolution.
		ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 16);
		ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);

		ledc_set_freq(ledc_timer.speed_mode, ledc_timer.timer_num, abs(speed));
	}
}

#endif /* MAIN_STEPPER_H_ */
