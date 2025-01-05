/*
 * wifi.h
 *
 *  Created on: Jan 27, 2023
 *      Author: yuexu
 */

#ifndef MAIN_WIFI_H_
#define MAIN_WIFI_H_

#include "config.h"
#include "config_store.h"
#include <driver/gpio.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <sdkconfig.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define WIFI_TAG "wifi"

typedef enum {
  NOTHING = 0,
  AP_OK,
  STA_ING,
  STA_OK,
  STA_ERROR,
} MY_STATUS;
static MY_STATUS wifi_status = NOTHING;

static int wifi_inited = 0;
static int s_retry_num = 0;
void _wifi_event_handler(void *arg, esp_event_base_t event_base,
                         int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t *event =
        (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(WIFI_TAG, "station %02x:%02x:%02x:%02x:%02x:%02x join, AID=%d",
             event->mac[0], event->mac[1], event->mac[2], event->mac[3],
             event->mac[4], event->mac[5], event->aid);
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t *event =
        (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(WIFI_TAG, "station %02x:%02x:%02x:%02x:%02x:%02x leave, AID=%d",
             event->mac[0], event->mac[1], event->mac[2], event->mac[3],
             event->mac[4], event->mac[5], event->aid);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(WIFI_TAG, "start connect to the AP");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
    ESP_LOGI(WIFI_TAG, "stop connect to the AP");
    wifi_status = STA_ERROR;
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    wifi_status = STA_OK;
  } else {
    ESP_LOGI(WIFI_TAG, "event_id: %d", (int)event_id);
  }
}

static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;
static esp_netif_t *nf;
void _wifi_init_ap() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  nf = esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &_wifi_event_handler, NULL,
      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &_wifi_event_handler, NULL,
      &instance_got_ip));

  wifi_config_t wifi_config = {.ap = {
                                   .authmode = WIFI_AUTH_WPA_WPA2_PSK,
                                   .max_connection = 4,
                                   .ssid = AP_NAME,
                                   .password = AP_PASSWORD,
                               }};

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(WIFI_TAG, "wifi init ssid:(%s), pass:(%s)", wifi_config.ap.ssid,
           wifi_config.ap.password);
  wifi_inited = 1;
}

void _wifi_init_sta(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  nf = esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &_wifi_event_handler, NULL,
      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &_wifi_event_handler, NULL,
      &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta =
          {
              .threshold.authmode = WIFI_AUTH_WPA_PSK,
          },
  };
  strcpy((char *)wifi_config.sta.ssid, AP_NAME);
  strcpy((char *)wifi_config.sta.password, AP_PASSWORD);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  int8_t p;
  esp_wifi_get_max_tx_power(&p);
  ESP_LOGI(WIFI_TAG, "1. max power: %d", p);
  esp_wifi_set_max_tx_power(80);
  esp_wifi_get_max_tx_power(&p);
  ESP_LOGI(WIFI_TAG, "2. max power: %d", p);

  ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");
  wifi_inited = 1;
}

void _deinit() {
  if (wifi_inited == 0)
    return;
  esp_wifi_stop();
  esp_wifi_deinit();
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      WIFI_EVENT, IP_EVENT_STA_GOT_IP, &instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &instance_any_id));
  esp_netif_destroy_default_wifi(nf);
  esp_event_loop_delete_default();
  esp_netif_deinit();
  wifi_inited = 0;
}

static int _server_mode;
void init_wifi(int server) {
  init_fs();
  _server_mode = server;
}

static int log_wait = 0;
void run_wifi() {
  if (log_wait == 0)
    ESP_LOGI(WIFI_TAG, "status now: %d", wifi_status);
  if (wifi_status == NOTHING && _server_mode == 1) {
    _deinit();
    _wifi_init_ap();
    ESP_LOGI(WIFI_TAG, "AP_OK, http server started");
    wifi_status = AP_OK;
  } else if (wifi_status == AP_OK) {
    if (log_wait == 0)
      ESP_LOGI(WIFI_TAG, "AP_OK");
  } else if (wifi_status == NOTHING && _server_mode == 0) {
    _deinit();
    _wifi_init_sta();
    wifi_status = STA_ING;
  } else if (wifi_status == STA_ING) {
    if (log_wait == 0)
      ESP_LOGI(WIFI_TAG, "STA_ING");
  } else if (wifi_status == STA_OK) {
    if (log_wait == 0)
      ESP_LOGI(WIFI_TAG, "STA_OK");
  } else if (wifi_status == STA_ERROR) {
    _deinit();
    ESP_LOGI(WIFI_TAG, "sta error, set nothing");
    wifi_status = NOTHING;
  }
  log_wait++;
  if (log_wait > 15)
    log_wait = 0;
}

#endif /* MAIN_WIFI_H_ */
