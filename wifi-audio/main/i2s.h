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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define I2S_TAG "i2s"

#define I2S_BUF_SIZE 512

typedef struct I2S_BUF {
	size_t len;
	uint32_t index;
	int8_t buf[I2S_BUF_SIZE];
} i2s_buf_t;

i2s_buf_t i2s_buf;

static i2s_chan_handle_t tx_chan;
static i2s_chan_handle_t rx_chan;

#define B24 0x7FFFFF
#define B16 0x7FFF
static float B2416 = 256;

static size_t _index = 0;

/*
static void print_buf(uint8_t type) {
	int* bf = (int*)i2s_buf.buf;
	size_t len = i2s_buf.len / 4;
	size_t j = 0;
	ESP_LOGE(I2S_TAG, "(%d) (%d) start print(%d)", type, (int)i2s_buf.len, (int)i2s_buf.index);
	for (j = 0; j < len; ++j) {
		ESP_LOGE(I2S_TAG, "%d", bf[j]);
	}
	ESP_LOGE(I2S_TAG, "(%d) end print (%d)", type, (int)i2s_buf.index);
}
*/

static void compress_buf() {
	int32_t* buf = (int32_t*)i2s_buf.buf;
	size_t len = i2s_buf.len / 4;
	uint32_t j;
	for (j = 0; j < len; j += 2) {
		int32_t data1 = (buf[j] << 1) >> 8; //i2s标准，右7位为空
		int32_t data2 = (buf[j + 1] << 1) >> 8;
		float fdata1 = (float)data1 / B2416;
		float fdata2 = (float)data2 / B2416;
		buf[j / 2] = ((int)fdata1 << 16) | ((int16_t)fdata2 & 0xFFFF);
	}
	i2s_buf.len /= 2;
}

static void decompress_buf() {
	int8_t _buf[I2S_BUF_SIZE];
	memcpy(_buf, i2s_buf.buf, I2S_BUF_SIZE);
	int32_t* buf = (int32_t*)_buf;
	int32_t* i2s_b = (int32_t*)i2s_buf.buf;
	size_t len = i2s_buf.len / 4;
	uint32_t j;
	for (j = 0; j < len; j++) {
		int32_t data1 = (buf[j] >> 16);
		int32_t data2 = (buf[j] << 16) >> 16;
		float fdata1 = (float)data1 * B2416;
		float fdata2 = (float)data2 * B2416;
		i2s_b[j * 2] = ((int)fdata1 << 7) & 0x7FFFFFFF;
		i2s_b[j * 2 + 1] = ((int)fdata2 << 7) & 0x7FFFFFFF;
	}
	i2s_buf.len *= 2;
}

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

static ssize_t i2s_read() {
  size_t r_bytes = 0;
  size_t _len = 0;
  while (1) {
	  if (i2s_channel_read(rx_chan, i2s_buf.buf + _len, I2S_BUF_SIZE - _len, (size_t *)&r_bytes,
	                       1000) != ESP_OK) {
	    return -1;
	  }
	  _len += r_bytes;
	  if (_len == I2S_BUF_SIZE) break;
  }
  i2s_buf.index = _index++;
  i2s_buf.len = I2S_BUF_SIZE;
  if (_index > 100000000) _index = 0;
  
  //MOCK DATA
  /*
  int* md = (int*)i2s_buf.buf;
  int j = 0;
md[j++] = 2144704384;
md[j++] = 2147345536;
md[j++] = 301056;
md[j++] = 2144376448;
md[j++] = 1913088;
md[j++] = 678784;
md[j++] = 2147167232;
md[j++] = 2146136960;
md[j++] = 2500480;
md[j++] = 1100544;
md[j++] = 1632256;
md[j++] = 2145201280;
md[j++] = 2146752512;
md[j++] = 2147283200;
md[j++] = 1207936;
md[j++] = 1664256;
md[j++] = 2146590208;
md[j++] = 1382016;
md[j++] = 848384;
md[j++] = 2146185984;
  r_bytes = j * 4;
  i2s_buf[i].len = r_bytes;
*/
  return i2s_buf.len;
}

static void i2s_write_init() {
  i2s_chan_config_t tx_chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
  ESP_LOGI(I2S_TAG, "i2s write inited");
}

static ssize_t i2s_write() {
  size_t w_size = 0;
  size_t _len = 0;
  if (i2s_buf.len == 0) return 0;
  while (1) {
	  if (i2s_channel_write(tx_chan, i2s_buf.buf + _len, i2s_buf.len - _len, (size_t *)&w_size, 1000) !=
	      ESP_OK) {
	    return -1;
	  }
	  _len += w_size;
	  if (_len == i2s_buf.len) {
		  break;
	  }
  }
  _index = i2s_buf.index;
  return _len;
}

#endif /* MAIN_I2S_H_ */
