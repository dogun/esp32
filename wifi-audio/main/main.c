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

#define SERVER_MODE 0

static SemaphoreHandle_t i2s_sem[I2S_BUF_CNT];
static SemaphoreHandle_t tcp_sem[I2S_BUF_CNT];

static const char *TAG = "wifi-audio";

static size_t tcp_total_len = 0;
static size_t i2s_total_len = 0;

static void i2s_write_task(void *pvParameters) {
  uint8_t i = 0;
  size_t idx = 0;
  while (1) {
    if (i >= I2S_BUF_CNT)
      i = 0;
    xSemaphoreTake(i2s_sem[i], portMAX_DELAY);
    //_apply_biquads_r(i2s_buf, i2s_buf, len / sizeof(int32_t));
    //_apply_biquads_l(i2s_buf, i2s_buf, len / sizeof(int32_t));
    if (i2s_buf[i].len <= 0) { // 还未收到数据
      vTaskDelay(100 / portTICK_PERIOD_MS);
    } else {
      size_t w_size = i2s_write(i);
      i2s_total_len += i2s_buf[i].len;
      size_t len = i2s_buf[i].len;
      if (w_size != len) {
        ESP_LOGE(TAG, "i2s write failed: errno %d, sent size %d, len %d", errno,
                 w_size, i2s_buf[i].len);
      }
      if (i2s_buf[i].index - idx != 1) {
        ESP_LOGE(TAG, "=====================%d %d", idx, (int)i2s_buf[i].index);
      }
      idx = i2s_buf[i].index;
    }
    xSemaphoreGive(tcp_sem[i]); // 可以去讀網絡了

    i++;
  }
  vTaskDelete(NULL);
}

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
  uint8_t i = 0;
  ssize_t err = 0;
  while (1) {
    if (i >= I2S_BUF_CNT)
      i = 0;
    xSemaphoreTake(tcp_sem[i], portMAX_DELAY);
    err = read_tcp(sock, &i2s_buf[i].len, sizeof(size_t));
    if (err > 0) {
      err = read_tcp(sock, &i2s_buf[i].index, sizeof(uint32_t));
      if (err > 0) {
        err = read_tcp(sock, i2s_buf[i].buf, i2s_buf[i].len);
      }
    }
    if (err <= 0) {
      ESP_LOGE(TAG, "read tcp error: %d %d", errno, err);
      xSemaphoreGive(i2s_sem[i]); // 也釋放i2s sem
      break;
    }
    tcp_total_len += i2s_buf[i].len;
    // print_buf(i, 3);
    decompress_buf(i);
    // print_buf(i, 4);
    xSemaphoreGive(i2s_sem[i]);
    i++;
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

static void i2s_read_task(void *pvParameters) {
  uint8_t i = 0;
  while (1) {
    if (i >= I2S_BUF_CNT)
      i = 0;
    xSemaphoreTake(i2s_sem[i], portMAX_DELAY);
    size_t r_size = i2s_read(i);
    i2s_total_len += r_size;
    // print_buf(i, 0);
    compress_buf(i);
    // print_buf(i, 1);
    // decompress_buf(i);
    // compress_buf(i, 2);
    xSemaphoreGive(tcp_sem[i]); // 可以去寫入網絡了
    if (r_size <= 0) {
      ESP_LOGE(TAG, "i2s read failed: errno %d, size %d", errno, r_size);
    }
    i++;
  }
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

      uint8_t i = 0;
      while (1) {
        if (i >= I2S_BUF_CNT)
          i = 0;
        xSemaphoreTake(tcp_sem[i], portMAX_DELAY);
        if (i2s_buf[i].len == 0) { // i2s還未準備好
          vTaskDelay(100 / portTICK_PERIOD_MS);
        } else {
          ssize_t err = send_tcp(sock, &i2s_buf[i], sizeof(i2s_buf[i]));
          tcp_total_len += (err - sizeof(size_t) - sizeof(uint32_t));
          if (err <= 0) {
            ESP_LOGE(TAG,
                     "Error occurred during sending: errno %d, len %d, sent %d",
                     errno, i2s_buf[i].len, err);
            xSemaphoreGive(i2s_sem[i]); // 可以繼續讀i2s了
            break;
          } else if (err < i2s_buf[i].len) {
            ESP_LOGE(TAG, "tcp write error: %d %d", err, i2s_buf[i].len);
          }
        }
        xSemaphoreGive(i2s_sem[i]); // 可以繼續讀i2s了
        i++;
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
  uint8_t j = 0;
  for (j = 0; j < I2S_BUF_CNT; ++j) {
    i2s_sem[j] = xSemaphoreCreateBinary();
    tcp_sem[j] = xSemaphoreCreateBinary();
  }

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
    xTaskCreatePinnedToCore(i2s_read_task, "i2s_read", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tcp_client_task, "tcp_client", 8192, NULL, 5, NULL,
                            0);

    vTaskDelay(500 / portTICK_PERIOD_MS);
    for (j = 0; j < I2S_BUF_CNT; ++j) {
      xSemaphoreGive(
          tcp_sem
              [j]); // 雖然是i2s讀取再寫入tcp，但是網絡準備好后才進行i2s讀取比較合適
    }
  } else {
    load_eq();
    xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 8192, NULL, 5, NULL,
                            0);
    xTaskCreatePinnedToCore(i2s_write_task, "i2s_write", 8192, NULL, 5, NULL,
                            1);

    vTaskDelay(500 / portTICK_PERIOD_MS);

    for (j = 0; j < I2S_BUF_CNT; ++j) {
      xSemaphoreGive(tcp_sem[j]); // 要開始讀取
    }

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
