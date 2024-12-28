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

#include "i2s.h"
#include "wifi.h"
#include "eq.h"
#include "eq_config.h"

#define SERVER_MODE 0

#define SETTING_PIN GPIO_NUM_33

static const char *TAG = "wifi-audio";

static void udp_server_task(void *pvParameters) {
  int addr_family = AF_INET;
  int ip_protocol = 0;
  struct sockaddr_in dest_addr;

  int port = 12345;

  i2s_write_init();

  while (1) {
    if (wifi_status != AP_OK) {
      ESP_LOGI(TAG, "server: wait wifi");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      continue;
    }

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      break;
    }
    ESP_LOGI(TAG, "Socket created");

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      break;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", port);

    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (1) {
      ESP_LOGI(TAG, "Waiting for data");
      int len = recvfrom(sock, i2s_buf, sizeof(i2s_buf) * sizeof(int32_t), 0,
                         (struct sockaddr *)&source_addr, &socklen);
      if (len < 0) {
        ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
        break;
      } else {
        ESP_LOGI(TAG, "Received %d bytes", len);
		_apply_biquads_r(i2s_buf, i2s_buf, len / sizeof(int32_t));
		_apply_biquads_l(i2s_buf, i2s_buf, len / sizeof(int32_t));
        int w_size = i2s_write(len);
        if (w_size != len) {
          ESP_LOGE(TAG, "i2s write failed: errno %d, size %d", errno, w_size);
        }
      }
    }

    if (sock != -1) {
      ESP_LOGE(TAG, "Shutting down socket and restarting...");
      shutdown(sock, 0);
      close(sock);
    }
  }
  vTaskDelete(NULL);
}

static void udp_client_task(void *pvParameters) {
  char host_ip[] = "192.168.4.1";
  int port = 12345;
  int addr_family = 0;
  int ip_protocol = 0;

  init_i2s_read();

  while (1) {
    if (wifi_status != STA_OK) {
      ESP_LOGI(TAG, "client: wait wifi");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      break;
    }

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    ESP_LOGI(TAG, "Socket created, sending to %s:%d", host_ip, port);

    while (1) {
      int r_size = i2s_read();
      if (r_size <= 0) {
        ESP_LOGE(TAG, "i2s read error: %d %d", r_size, errno);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        r_size = 32;
        sendto(sock, i2s_buf, r_size, 0, (struct sockaddr *)&dest_addr,
                       sizeof(dest_addr));
        ESP_LOGI(TAG, "sent1: %d", r_size);
        continue;
      }
      int err = sendto(sock, i2s_buf, r_size, 0, (struct sockaddr *)&dest_addr,
                       sizeof(dest_addr));
      if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        break;
      }
      ESP_LOGI(TAG, "sent: %d", r_size);
    }

    if (sock != -1) {
      ESP_LOGE(TAG, "Shutting down socket and restarting...");
      shutdown(sock, 0);
      close(sock);
    }
  }
  vTaskDelete(NULL);
}

void app_main(void) {
  gpio_reset_pin(SETTING_PIN);
  gpio_set_direction(SETTING_PIN, GPIO_MODE_INPUT);

  init_wifi();
  i2s_init_std_duplex();

  if (SERVER_MODE == 0) {
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
  } else {
    save_config(CONFIG_SSID, "");
    save_config(CONFIG_PASSWORD, "");
    save_config(CONFIG_UID, "");
    load_eq();
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
  }
  int reconfiging = 0;
  while (true) {
	int setting = gpio_get_level(SETTING_PIN);
	setting = 0;
	if (setting == 1) {
		if (reconfiging == 0) {
			reconfig_wifi();
			reconfiging = 1;
		}
		run_wifi(0);
	} else {
    	run_wifi(SERVER_MODE);
    }
    sleep(1);
  }
}
