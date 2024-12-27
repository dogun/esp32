#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>
#include <sys/param.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "wifi.h"

#define SERVER_PIN GPIO_NUM_33

static const char *TAG = "wifi-audio";
static const char *payload = "12345678";

static void udp_server_task(void *pvParameters) {
  char rx_buffer[128];
  char addr_str[128];
  int addr_family = AF_INET;
  int ip_protocol = 0;
  struct sockaddr_in dest_addr;

  int port = 12345;

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
      int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                         (struct sockaddr *)&source_addr, &socklen);
      if (len < 0) {
        ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
        break;
      } else {
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str,
                    sizeof(addr_str) - 1);

        rx_buffer[len] = 0;
        ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
        ESP_LOGI(TAG, "%s", rx_buffer);

        int err = sendto(sock, rx_buffer, len, 0,
                         (struct sockaddr *)&source_addr, sizeof(source_addr));
        if (err < 0) {
          ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
          break;
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
  char rx_buffer[128];
  char host_ip[] = "192.168.4.1";
  int port = 12345;
  int addr_family = 0;
  int ip_protocol = 0;

  while (1) {
    if (wifi_status != STA_OK) {
      ESP_LOGI(TAG, "client: wait wifi");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
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

      int err = sendto(sock, payload, strlen(payload), 0,
                       (struct sockaddr *)&dest_addr, sizeof(dest_addr));
      if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        break;
      }
      ESP_LOGI(TAG, "Message sent");

      struct sockaddr_storage source_addr;
      socklen_t socklen = sizeof(source_addr);
      int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                         (struct sockaddr *)&source_addr, &socklen);

      if (len < 0) {
        ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
        break;
      } else {
        rx_buffer[len] = 0;
        ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
        ESP_LOGI(TAG, "%s", rx_buffer);
        if (strncmp(rx_buffer, "OK: ", 4) == 0) {
          ESP_LOGI(TAG, "Received expected message, reconnecting");
          break;
        }
      }

      vTaskDelay(2000 / portTICK_PERIOD_MS);
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
  gpio_reset_pin(SERVER_PIN);
  gpio_set_direction(SERVER_PIN, GPIO_MODE_INPUT);

  init_wifi();

  int is_server = gpio_get_level(SERVER_PIN);
  is_server = 1;

  if (!is_server) {
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
  } else {
    save_config(CONFIG_SSID, "");
    save_config(CONFIG_PASSWORD, "");
    save_config(CONFIG_UID, "");
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
  }
  while (true) {
    run_wifi(is_server);
    sleep(1);
  }
}
