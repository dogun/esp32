/*
 * tcp.h
 *
 *  Created on: 2025年1月10日
 *      Author: D
 */

#ifndef MAIN_TCP_H_
#define MAIN_TCP_H_

#include "config.h"
#include "eq.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "i2s.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "portmacro.h"
#include "wifi.h"
#include <lwip/netdb.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

static const char *TCP_TAG = "tcp";

static size_t send_err_12 = 0;
static size_t recv_err_11 = 0;

static size_t net_total_len = 0;

static ssize_t _read(const int sock, void *buf, size_t len, int mod) {
  size_t _len = 0;
  ssize_t err = 0;
  while (1) {
    err = recv(sock, buf + _len, len - _len, mod);
    if (err < 0) {
      break;
    }
    _len += err;
    if (_len == len)
      break;
  }
  return err;
}

static size_t _last_index = 0;
static void net_server_task(void *pvParameters) {
  i2s_write_init();

  while (1) {
    if (wifi_status != STA_OK) {
      ESP_LOGI(TCP_TAG, "server: wait wifi");
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

    int l_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (l_sock < 0) {
      ESP_LOGE(TCP_TAG, "Unable to create socket: errno %d", errno);
      break;
    }
    ESP_LOGI(TCP_TAG, "Socket created");

    int opt = 1;
    setsockopt(l_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int err = bind(l_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
      ESP_LOGE(TCP_TAG, "Socket unable to bind: errno %d", errno);
      goto CLEAN_UP;
    }
    ESP_LOGI(TCP_TAG, "Socket bound, port %d", SERVER_PORT);

    err = listen(l_sock, 1);
    if (err != 0) {
      ESP_LOGE(TCP_TAG, "Error occurred during listen: errno %d", errno);
      goto CLEAN_UP;
    }
    ESP_LOGI(TCP_TAG, "Socket listening");

    while (1) {
      struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
      socklen_t addr_len = sizeof(source_addr);
      int sock = accept(l_sock, (struct sockaddr *)&source_addr, &addr_len);
      if (sock < 0) {
        ESP_LOGE(TCP_TAG, "Unable to accept connection: errno %d", errno);
        break;
      }
      ESP_LOGI(TCP_TAG, "Socket accepted");

      // Set timeout
      struct timeval timeout;
      timeout.tv_sec = 2;
      timeout.tv_usec = 0;
      setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

      while (1) {
        ssize_t len = _read(sock, &i2s_buf.len, sizeof(i2s_buf.len), MSG_PEEK);
        if (len < 0) {
          ESP_LOGE(TCP_TAG, "recv buf len error: errno %d, len %d", errno, len);
          break;
        }
        if (i2s_buf.len > I2S_BUF_SIZE) {
          ESP_LOGE(TCP_TAG, "len recv error: len %d", len);
          break;
        }
        len =
            _read(sock, &i2s_buf,
                  sizeof(i2s_buf.index) + sizeof(i2s_buf.len) + i2s_buf.len, 0);
        // Error occurred during receiving
        if (len < 0) {
          ESP_LOGE(TCP_TAG, "recv failed: errno %d", errno);
          break;
        }
        net_total_len += i2s_buf.len;

        // print_buf(i, 3);
        decompress_buf();
        _biquads_x((int32_t *)i2s_buf.buf, (int32_t *)i2s_buf.buf,
                   i2s_buf.len / 4);
        // print_buf(i, 4);

        // 判断下index
        if (i2s_buf.index - _last_index != 1) {
          ESP_LOGE(TCP_TAG, "i2s index error: %d %d", i2s_buf.index,
                   _last_index);
        }

        ssize_t w_size = i2s_write();
        if (w_size < 0) {
          ESP_LOGE(TCP_TAG, "write i2s error: %d %d", errno, err);
        }
        _last_index = i2s_buf.index;

        i2s_total_len += i2s_buf.len;
      }

      if (sock != -1) {
        ESP_LOGE(TCP_TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
    }
  CLEAN_UP:
    close(l_sock);
    vTaskDelete(NULL);
  }
}

static ssize_t _send(int sock, void *buf, size_t len,
                     struct sockaddr *dest_addr, size_t dest_addr_len) {
  size_t _len = 0;
  ssize_t err;
  if (len == 0) {
    return 0;
  }
  while (1) {
    err = send(sock, buf + _len, len - _len, 0);
    if (err < 0) {
      break;
    }
    _len += err;
    if (_len == len) {
      break;
    }
  }
  return err;
}

static void net_client_task(void *pvParameters) {
  init_i2s_read();

  // Set timeout
  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;

  while (1) {
    if (wifi_status != STA_OK) {
      ESP_LOGI(TCP_TAG, "client: wait wifi");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    // 域名解析
    struct hostent *server_info;
    while (1) {
      ESP_LOGI(TCP_TAG, "query ip: %s", SERVER_IP);

      server_info = gethostbyname(SERVER_IP);
      if (server_info) {
        break;
      }
    }
    // 获取服务器IP地址
    uint8_t server_ip[4];
    bcopy(server_info->h_addr_list[0], server_ip, sizeof(server_ip));

    ESP_LOGI(TCP_TAG, "(OK)query ip: %d %d %d %d", server_ip[0], server_ip[1],
             server_ip[2], server_ip[3]);

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr =
        inet_addr(inet_ntoa(*(struct in_addr *)server_ip));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERVER_PORT);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
      ESP_LOGE(TCP_TAG, "Unable to create socket: errno %d", errno);
      break;
    }

    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);

    ESP_LOGI(TCP_TAG, "Socket created");

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
      ESP_LOGE(TCP_TAG, "Socket unable to connect: errno %d", errno);
      break;
    }
    ESP_LOGI(TCP_TAG, "Successfully connected");

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
      if (err < 0) {
        ESP_LOGE(
            TCP_TAG, "Error occurred during sending: errno %d, len %d, sent %d",
            errno, i2s_buf.len + sizeof(i2s_buf.index) + sizeof(i2s_buf.len),
            err);
        break;
      } else if (err < i2s_buf.len) {
        ESP_LOGE(TCP_TAG, "write error: %d %d", err, i2s_buf.len);
        break;
      }
      net_total_len += (err - sizeof(size_t) - sizeof(uint32_t));
    }

    if (sock != -1) {
      ESP_LOGE(TCP_TAG, "Shutting down socket and restarting...");
      shutdown(sock, 0);
      close(sock);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
  vTaskDelete(NULL);
}

#endif /* MAIN_TCP_H_ */
