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
#include "eq_config.h"
#include "i2s.h"
#include "wifi.h"

#define SERVER_MODE 1

#define SETTING_PIN GPIO_NUM_33

static const char *TAG = "wifi-audio";

static void do_retransmit(const int sock) {
  int len;
  do {
    len = recv(sock, i2s_buf, sizeof(i2s_buf), 0);
    if (len < 0) {
      ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
    } else if (len == 0) {
      ESP_LOGW(TAG, "Connection closed");
    } else {
      //ESP_LOGI(TAG, "Received %d bytes", len);
      //_apply_biquads_r(i2s_buf, i2s_buf, len / sizeof(int32_t));
      //_apply_biquads_l(i2s_buf, i2s_buf, len / sizeof(int32_t));
      int w_size = i2s_write(len);
      if (w_size != len) {
        ESP_LOGE(TAG, "i2s write failed: errno %d, size %d", errno, w_size);
      }
    }
  } while (len > 0);
}

static void tcp_server_task(void *pvParameters) {
  i2s_write_init();

  while (1) {
    if (wifi_status != AP_OK) {
      ESP_LOGI(TAG, "server: wait wifi");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
      break;
    }
  }

  char addr_str[128];

  int port = 12345;
  int keepAlive = 1;
  int keepIdle = 10;
  int keepInterval = 10;
  int keepCount = 10;
  struct sockaddr_storage dest_addr;

  struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
  dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr_ip4->sin_family = AF_INET;
  dest_addr_ip4->sin_port = htons(port);

  int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (listen_sock < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }
  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  ESP_LOGI(TAG, "Socket created");

  int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err != 0) {
    ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    goto CLEAN_UP;
  }
  ESP_LOGI(TAG, "Socket bound, port %d", port);

  err = listen(listen_sock, 5);
  if (err != 0) {
    ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
    goto CLEAN_UP;
  }

  while (1) {
    ESP_LOGI(TAG, "Socket listening");

    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
      break;
    }

    // Set tcp keepalive option
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
    // Convert ip address to string
    if (source_addr.ss_family == PF_INET) {
      inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str,
                  sizeof(addr_str) - 1);
    }

    ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

    do_retransmit(sock);

    shutdown(sock, 0);
    close(sock);
  }

CLEAN_UP:
  close(listen_sock);
  vTaskDelete(NULL);
}

static void tcp_client_task(void *pvParameters) {
  char host_ip[] = "192.168.4.1";
  int port = 12345;

  struct sockaddr_in dest_addr;
  dest_addr.sin_addr.s_addr = inet_addr(host_ip);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(port);

  init_i2s_read();

  while (1) {
    if (wifi_status != STA_OK) {
      ESP_LOGI(TAG, "client: wait wifi");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      break;
    }

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
      ESP_LOGI(TAG, "Successfully connected");

      while (1) {
        int r_size = i2s_read();
        if (r_size <= 0) {
          ESP_LOGE(TAG, "i2s read error: %d %d", r_size, errno);
          continue;
        }
        int err = send(sock, i2s_buf, r_size, 0);
        if (err < 0) {
          ESP_LOGE(TAG,
                   "Error occurred during sending: errno %d, len %d, sent %d",
                   errno, r_size, err);
          continue;
        }
        //ESP_LOGI(TAG, "sent: %d", r_size);
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

void app_main(void) {
  gpio_reset_pin(SETTING_PIN);
  gpio_set_direction(SETTING_PIN, GPIO_MODE_INPUT);

  gpio_reset_pin(GPIO_NUM_16);
  gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
  gpio_reset_pin(GPIO_NUM_17);
  gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
  gpio_reset_pin(GPIO_NUM_18);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);

  gpio_set_level(GPIO_NUM_16, 1);
  gpio_set_level(GPIO_NUM_17, 0);
  gpio_set_level(GPIO_NUM_18, 0);

  init_wifi();

  if (SERVER_MODE == 0) {
    xTaskCreatePinnedToCore(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL,
                            1);
    gpio_set_level(GPIO_NUM_18, 1);
  } else {
    save_config(CONFIG_SSID, "");
    save_config(CONFIG_PASSWORD, "");
    save_config(CONFIG_UID, "");
    load_eq();
    xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL,
                            1);
    gpio_set_level(GPIO_NUM_17, 1);
    gpio_set_level(GPIO_NUM_16, 0);
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