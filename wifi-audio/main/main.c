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

static const char *TAG = "wifi-audio";

static size_t udp_total_len = 0;
static size_t i2s_total_len = 0;

static size_t send_err_12 = 0;
static size_t recv_err_11 = 0;

static ssize_t _read(const int sock, void *buf, size_t len, int mod) {
  size_t _len = 0;
  ssize_t err = 0;
  while (1) {
    err = recv(sock, buf + _len, len - _len, mod);
    if (err < 0 && errno == 11) {
      recv_err_11++;
      continue; // 如果缓冲区没数据，则重试
    } else if (err < 0) {
      break;
    }
    _len += err;
    if (_len == len)
      break;
  }
  return err;
}


static size_t _last_index = 0;
static void udp_server_task(void *pvParameters) {
  i2s_write_init();

  while (1) {
    if (wifi_status != STA_OK) {
      ESP_LOGI(TAG, "server: wait wifi");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
      break;
    }
  }

  while (1) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERVER_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      break;
    }
    ESP_LOGI(TAG, "Socket created");

    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    }
    ESP_LOGI(TAG, "Socket bound, port %d", SERVER_PORT);

    while (1) {
      ssize_t len = _read(sock, &i2s_buf.len, sizeof(i2s_buf.len), MSG_PEEK);
      if (len != sizeof(i2s_buf.len)) {
        ESP_LOGE(TAG, "recvfrom buf len error: errno %d, len %d", errno, len);
        continue;
      }
      if (i2s_buf.len > I2S_BUF_SIZE) {
        ESP_LOGE(TAG, "len recv error: len %d", len);
        break;
      }
      len = _read(sock, &i2s_buf,
                  sizeof(i2s_buf.index) + sizeof(i2s_buf.len) + i2s_buf.len, 0);
      // Error occurred during receiving
      if (len < 0) {
        ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
        break;
      }
      if (len < i2s_buf.len) {
        ESP_LOGE(TAG, "recvfrom failed: len %d %d", len, i2s_buf.len);
        break;
      }
      udp_total_len += i2s_buf.len;
      // print_buf(i, 3);
      decompress_buf();
      // print_buf(i, 4);
      
      //判断下index
      if (i2s_buf.index - _last_index != 1) {
		  ESP_LOGE(TAG, "i2s index error: %d %d", i2s_buf.index, _last_index);
	  }
      
      ssize_t w_size = i2s_write();
      if (w_size < 0) {
        ESP_LOGE(TAG, "write i2s error: %d %d", errno, err);
      }
      _last_index = i2s_buf.index;

      i2s_total_len += i2s_buf.len;
    }

    if (sock != -1) {
      ESP_LOGE(TAG, "Shutting down socket and restarting...");
      shutdown(sock, 0);
      close(sock);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
  vTaskDelete(NULL);
}

static ssize_t _send(int sock, void *buf, size_t len,
                     struct sockaddr *dest_addr, size_t dest_addr_len) {
  size_t _len = 0;
  ssize_t err;
  if (len == 0) {
    return 0;
  }
  while (1) {
    err = sendto(sock, buf + _len, len - _len, 0, dest_addr, dest_addr_len);
    if (err < 0 && errno == 12) {
      send_err_12++;
      continue; // 缓冲区不足则重试
    } else if (err < 0) {
      break;
    }
    _len += err;
    if (_len == len) {
      break;
    }
  }
  return err;
}

static void udp_client_task(void *pvParameters) {
  init_i2s_read();

  // Set timeout
  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;

  while (1) {
    if (wifi_status != STA_OK) {
      ESP_LOGI(TAG, "client: wait wifi");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    // 域名解析
    struct hostent *server_info;
    while (1) {
      ESP_LOGI(TAG, "query ip: %s", SERVER_IP);

      server_info = gethostbyname(SERVER_IP);
      if (server_info) {
        ESP_LOGI(TAG, "(OK)query ip: %s", SERVER_IP);
        break;
      }
    }
    // 获取服务器IP地址
    uint8_t server_ip[4];
    bcopy(server_info->h_addr_list[0], server_ip, sizeof(server_ip));

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr =
        inet_addr(inet_ntoa(*(struct in_addr *)server_ip));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERVER_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      break;
    }

    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);

    ESP_LOGI(TAG, "Socket created, sending to %s:%d", SERVER_IP, SERVER_PORT);

    while (1) {
      i2s_read();
      // i2s_buf.len = I2S_BUF_SIZE;
      i2s_total_len += i2s_buf.len;
      // print_buf(i, 0);
      compress_buf();
      ssize_t err =
          _send(sock, &i2s_buf,
                i2s_buf.len + sizeof(i2s_buf.index) + sizeof(i2s_buf.len),
                (struct sockaddr *)&dest_addr, sizeof(dest_addr));
      if (err <= 0) {
        ESP_LOGE(
            TAG, "Error occurred during sending: errno %d, len %d, sent %d",
            errno, i2s_buf.len + sizeof(i2s_buf.index) + sizeof(i2s_buf.len),
            err);
        break;
      } else if (err < i2s_buf.len) {
        ESP_LOGE(TAG, "write error: %d %d", err, i2s_buf.len);
        break;
      }
      udp_total_len += (err - sizeof(size_t) - sizeof(uint32_t));
    }

    if (sock != -1) {
      ESP_LOGE(TAG, "Shutting down socket and restarting...");
      shutdown(sock, 0);
      close(sock);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
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

  if (SERVER_MODE == 0) {
    gpio_set_level(GPIO_NUM_18, 1);
    xTaskCreatePinnedToCore(udp_client_task, "_client", 8192, NULL, 5, NULL, 1);
  } else {
    // load_eq();
    xTaskCreatePinnedToCore(udp_server_task, "_server", 8192, NULL, 5, NULL, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_17, 1);
    gpio_set_level(GPIO_NUM_16, 0);
  }

  size_t udp_last_len = 0;
  size_t i2s_last_len = 0;
  size_t last_err_12 = 0;
  size_t last_err_11 = 0;
  while (true) {
    run_wifi(SERVER_MODE);

    size_t i2s_t = i2s_total_len;
    size_t i2s_p = i2s_t - i2s_last_len;
    i2s_last_len = i2s_t;

    size_t udp_t = udp_total_len;
    size_t udp_p = udp_t - udp_last_len;
    udp_last_len = udp_t;

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
    if (udp_total_len > 100000000) {
      udp_total_len = 0;
      udp_last_len = 0;
    }
    if (send_err_12 > 100000000) {
      send_err_12 = 0;
      last_err_12 = 0;
    }
    if (recv_err_11 > 100000000) {
      recv_err_11 = 0;
      last_err_11 = 0;
    }

    ESP_LOGE(TAG, "udp: %d i2s: %d index: %d last: %d %d, neterr: %d %d", udp_p,
             i2s_p, _index, udp_last_len, i2s_last_len, e11, e12);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
