/*
 * i2s.h
 *
 *  Created on: 2024年12月28日
 *      Author: D
 */

#ifndef MAIN_I2S_H_
#define MAIN_I2S_H_

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/i2s_types.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <stdlib.h>

#define I2S_TAG "i2s"

static i2s_chan_handle_t tx_chan;
static i2s_chan_handle_t rx_chan;

typedef struct I2S_BUF {
	int8_t buf[2048];
	size_t len;
} i2s_buf_t;

i2s_buf_t i2s_buf[3];

static i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                I2S_SLOT_MODE_STEREO),
    .gpio_cfg =
        {
            .mclk = 0,
            .bclk = 27,
            .ws = 26,
            .dout = 25,
            .din = 23,
            .invert_flags =
                {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
        },
};

static void init_i2s_read() {

  i2s_chan_config_t rx_chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
  ESP_LOGI(I2S_TAG, "i2s read inited");
}

static int i2s_read(int i) {
  int r_bytes = 0;

  if (i2s_channel_read(rx_chan, i2s_buf[i].buf, sizeof(i2s_buf[i].buf), (size_t *)&r_bytes,
                       1000) != ESP_OK) {
    r_bytes = -1;
  }
  return r_bytes;
}

static void i2s_write_init() {
  i2s_chan_config_t tx_chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
  ESP_LOGI(I2S_TAG, "i2s write inited");
}

static int i2s_write(int i) {
  int w_size = 0;
  if (i2s_channel_write(tx_chan, i2s_buf[i].buf, i2s_buf[i].len, (size_t *)&w_size, 1000) !=
      ESP_OK) {
    w_size = -1;
  }
  // ESP_LOGI(I2S_TAG, "write: %d", w_size);
  return w_size;
}

#endif /* MAIN_I2S_H_ */
