/*
 * led_i2c.h
 *
 *  Created on: Sep 9, 2023
 *      Author: yuexu
 */

#include <stdio.h>
#include <string.h>
#include "driver/i2c.h"

#ifndef MAIN_LED_I2C_H_
#define MAIN_LED_I2C_H_

#define I2C_TAG "led-i2c"

#define I2C_MASTER_SCL_IO           19      /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           18      /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define PIN_NUM 10
#define BOARD_COUNT 2

int set_duty(int board_index, uint8_t pin_id, uint8_t duty) {
	ESP_LOGI(I2C_TAG, "set_duty, board:%d, pin:%d, duty:%d", board_index,
			pin_id, duty);
	uint8_t wb[3] = { 0 };
	wb[1] = pin_id;
	wb[2] = duty;
	int retry = 0;
	while (1) {
		esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM,
				board_index, wb, sizeof(wb),
				I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
		if (err != ESP_OK) {
			ESP_LOGE(I2C_TAG, "set_duty ERROR: %d, %s", err,
					esp_err_to_name(err));
		} else
			return 0;
		if (retry++ > 0)
			return -1;
	}
}

void read_from_board(int board_index, uint8_t* bf, size_t size) {
	esp_err_t err = i2c_master_read_from_device(I2C_MASTER_NUM,
			board_index, bf, size,
			I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	if (err != ESP_OK) {
		ESP_LOGE(I2C_TAG, "read board ERROR: %d, %s", err,
				esp_err_to_name(err));
	}
}

esp_err_t i2c_master_init(void) {
	int i2c_master_port = I2C_MASTER_NUM;

	i2c_config_t conf = {
			.mode = I2C_MODE_MASTER,
			.sda_io_num = I2C_MASTER_SDA_IO,
			.scl_io_num = I2C_MASTER_SCL_IO,
			.sda_pullup_en = GPIO_PULLUP_ENABLE,
			.scl_pullup_en = GPIO_PULLUP_ENABLE,
			.master.clk_speed = I2C_MASTER_FREQ_HZ,
	};

	i2c_param_config(i2c_master_port, &conf);

	return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

#endif /* MAIN_LED_I2C_H_ */
