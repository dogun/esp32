#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "eq.h"
#include "i2s.h"
#include "portmacro.h"
#include "wifi.h"

#ifdef TCP
	#include "tcp.h"
#else
	#include "udp.h"
#endif

static const char *TAG = "wifi-audio";

void app_main(void) {
  gpio_reset_pin(GPIO_NUM_16);
  gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
  gpio_reset_pin(GPIO_NUM_17);
  gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
  gpio_reset_pin(GPIO_NUM_18);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);

  gpio_set_level(GPIO_NUM_16, 1);
  gpio_set_level(GPIO_NUM_17, 0);
  gpio_set_level(GPIO_NUM_18, 0);

  if (SERVER_MODE == 0) {
    gpio_set_level(GPIO_NUM_18, 1);
    xTaskCreatePinnedToCore(net_client_task, "_client", 8192, NULL, 5, NULL, 1);
  } else {
    // load_eq();
    xTaskCreatePinnedToCore(net_server_task, "_server", 8192, NULL, 5, NULL, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_17, 1);
    gpio_set_level(GPIO_NUM_16, 0);
  }

  size_t net_last_len = 0;
  size_t i2s_last_len = 0;
  size_t last_err_12 = 0;
  size_t last_err_11 = 0;
  while (true) {
    run_wifi(SERVER_MODE);

    size_t i2s_t = i2s_total_len;
    size_t i2s_p = i2s_t - i2s_last_len;
    i2s_last_len = i2s_t;

    size_t net_t = net_total_len;
    size_t net_p = net_t - net_last_len;
    net_last_len = net_t;

    size_t err_12_t = send_err_12;
    size_t e12 = err_12_t - last_err_12;
    last_err_12 = err_12_t;

    size_t err_11_t = recv_err_11;
    size_t e11 = err_11_t - last_err_11;
    last_err_11 = err_11_t;

    if (i2s_total_len > 100000000) {
      i2s_total_len = 0;
      i2s_last_len = 0;
    }
    if (net_total_len > 100000000) {
      net_total_len = 0;
      net_last_len = 0;
    }
    if (send_err_12 > 100000000) {
      send_err_12 = 0;
      last_err_12 = 0;
    }
    if (recv_err_11 > 100000000) {
      recv_err_11 = 0;
      last_err_11 = 0;
    }

    ESP_LOGE(TAG, "net: %d i2s: %d index: %d last: %d %d, neterr: %d %d", net_p,
             i2s_p, _index, net_last_len, i2s_last_len, e11, e12);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
