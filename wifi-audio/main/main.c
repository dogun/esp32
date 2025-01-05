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
#include "portmacro.h"
#include "wifi.h"

#define SERVER_MODE 1

static const char *TAG = "wifi-audio";

static size_t tcp_total_len = 0;
static size_t i2s_total_len = 0;

static ssize_t read_tcp(const int sock, void *buf, size_t len) {
  size_t _len = 0;
  ssize_t err = 0;
  while (1) {
    err = recv(sock, buf + _len, len - _len, 0);
    if (err <= 0) {
      break;
    }
    _len += err;
    if (_len == len)
      break;
  }
  return err;
}

static void do_retransmit(const int sock) {
  ssize_t err = 0;
  while (1) {
    err = read_tcp(sock, &i2s_buf.len, sizeof(size_t));
    if (err > 0) {
      err = read_tcp(sock, &i2s_buf.index, sizeof(uint32_t));
      if (err > 0) {
        err = read_tcp(sock, i2s_buf.buf, i2s_buf.len);
      }
    }
    if (err <= 0) {
      ESP_LOGE(TAG, "read tcp error: %d %d", errno, err);
      break;
    }
    tcp_total_len += i2s_buf.len;
    // print_buf(i, 3);
    decompress_buf();
    // print_buf(i, 4);
    ssize_t w_size = i2s_write();
    if (w_size < 0) {
      ESP_LOGE(TAG, "write i2s error: %d %d", errno, err);
    }
    i2s_total_len += i2s_buf.len;
  }
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

    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

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

static ssize_t send_tcp(int sock, void *buf, size_t len) {
  size_t _len = 0;
  ssize_t err;
  if (len == 0) {
    return 0;
  }
  while (1) {
    err = send(sock, buf + _len, len - _len, 0);
    if (err <= 0) {
      break;
    }
    _len += err;
    if (_len == len) {
      break;
    }
  }
  return err;
}

static void tcp_client_task(void *pvParameters) {
  char host_ip[] = "192.168.4.1";
  uint32_t port = 12345;

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

    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
      ESP_LOGI(TAG, "Successfully connected");

      while (1) {
        size_t r_size = i2s_read();
        i2s_total_len += r_size;
        // print_buf(i, 0);
        compress_buf();
        ssize_t err = send_tcp(sock, &i2s_buf, i2s_buf.len + sizeof(i2s_buf.index) + sizeof(i2s_buf.len));
        if (err <= 0) {
          ESP_LOGE(TAG,
                   "Error occurred during sending: errno %d, len %d, sent %d",
                   errno, i2s_buf.len, err);
          break;
        } else if (err < i2s_buf.len) {
          ESP_LOGE(TAG, "tcp write error: %d %d", err, i2s_buf.len);
        }
        tcp_total_len += (err - sizeof(size_t) - sizeof(uint32_t));
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
  gpio_reset_pin(GPIO_NUM_16);
  gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
  gpio_reset_pin(GPIO_NUM_17);
  gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
  gpio_reset_pin(GPIO_NUM_18);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);

  gpio_set_level(GPIO_NUM_16, 1);
  gpio_set_level(GPIO_NUM_17, 0);
  gpio_set_level(GPIO_NUM_18, 0);

  init_wifi(SERVER_MODE);

  if (SERVER_MODE == 0) {
    gpio_set_level(GPIO_NUM_18, 1);
    xTaskCreatePinnedToCore(tcp_client_task, "tcp_client", 8192, NULL, 5, NULL,
                            1);
  } else {
    load_eq();
    xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 20480, NULL, 5, NULL,
                            1);

    vTaskDelay(500 / portTICK_PERIOD_MS);

    gpio_set_level(GPIO_NUM_17, 1);
    gpio_set_level(GPIO_NUM_16, 0);
  }

  size_t tcp_last_len = 0;
  size_t i2s_last_len = 0;
  while (true) {
    run_wifi();

    size_t i2s_t = i2s_total_len;
    size_t i2s_p = i2s_t - i2s_last_len;
    i2s_last_len = i2s_t;

    size_t tcp_t = tcp_total_len;
    size_t tcp_p = tcp_t - tcp_last_len;
    tcp_last_len = tcp_t;

    if (i2s_total_len > 100000000)
      i2s_total_len = 0;
    if (tcp_total_len > 100000000)
      tcp_total_len = 0;

    ESP_LOGE(TAG, "tcp: %d i2s: %d index: %d last: %d %d", tcp_p, i2s_p, _index,
             tcp_last_len, i2s_last_len);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
