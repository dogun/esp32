/* Blink Example

 This example code is in the Public Domain (or CC0 licensed, at your option.)

 Unless required by applicable law or agreed to in writing, this
 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

#define EN_GPIO 13
#define U1_GPIO 23
#define U2_GPIO 32
#define U3_GPIO 22
#define U4_GPIO 25
#define U5_GPIO 26
#define U6_GPIO 27

#define TAG "main"

#define TIME_OUT 600

static int on_gpio = -1;
static int on_time = 0;

static long now_time = 0;

void set_level(uint8_t port, int level) {
	gpio_reset_pin(port);
	gpio_set_direction(port, GPIO_MODE_OUTPUT);
	gpio_set_level(port, level);
}

void en_on() {
	set_level(EN_GPIO, 0);
}

void en_off() {
	set_level(EN_GPIO, 1);
}

void off_all() {
	en_off();
	set_level(EN_GPIO, 0);
	set_level(U1_GPIO, 0);
	set_level(U2_GPIO, 0);
	set_level(U3_GPIO, 0);
	set_level(U4_GPIO, 0);
	set_level(U5_GPIO, 0);
	set_level(U6_GPIO, 0);
}

void on_u(int num) {
	off_all();
	if (num == 1) {
		set_level(U1_GPIO, 1);
	} else if (num == 2) {
		set_level(U2_GPIO, 1);
	} else if (num == 3) {
		set_level(U3_GPIO, 1);
	} else if (num == 4) {
		set_level(U4_GPIO, 1);
	} else if (num == 5) {
		set_level(U5_GPIO, 1);
	} else if (num == 6) {
		set_level(U6_GPIO, 1);
	}
	on_time = now_time;
	on_gpio = num;
	en_on();
	ESP_LOGI(TAG, "on %d", num);
}

void _wifi_event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data) {
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event =
				(wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(TAG, "station %s join, AID=%d", (char*)event->mac,
				event->aid);
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event =
				(wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(TAG, "station %s leave, AID=%d",
				(char*)event->mac, event->aid);
	}
}

esp_err_t _get_handler_index(httpd_req_t* req) {
	char* usb_num_str = strstr(req->uri, "=");

	if (NULL == usb_num_str) {
		httpd_resp_send_404(req);
		return ESP_FAIL;
	}

	usb_num_str ++;
	int usb_num = atoi(usb_num_str);
	if (usb_num > 6 || usb_num < 1) {
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}

	on_u(usb_num);

	char* html = "%d=OK";
	char data[32] = {0};
	sprintf(data, html, usb_num);
	httpd_resp_send(req, data, strlen(data));
	return ESP_OK;
}

httpd_uri_t uri_get_index = {
		.uri = "/",
		.method = HTTP_GET,
		.handler = _get_handler_index,
		.user_ctx = NULL
};

httpd_handle_t start_webserver(void) {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	httpd_handle_t server = NULL;

	if (httpd_start(&server, &config) == ESP_OK) {
		httpd_register_uri_handler(server, &uri_get_index);
	}
	return server;
}

#define AP_NAME "USB-SW"
#define AP_PASSWORD "1234567890"

void app_main(void) {
	nvs_flash_init();

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_ap();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_wifi_event_handler, NULL, NULL));

	wifi_config_t wifi_config = { .ap = { .authmode = WIFI_AUTH_WPA_WPA2_PSK,
			.max_connection = 4, .ssid = AP_NAME, .password = AP_PASSWORD, } };

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "wifi init ssid:(%s), pass:(%s)", wifi_config.ap.ssid, wifi_config.ap.password);

	start_webserver();
	ESP_LOGI(TAG, "http server started");

	off_all();

	ESP_LOGI(TAG, "init finished");

	while (1) {
		ESP_LOGI(TAG, "running...");
		now_time += 1;

		if (on_gpio > 0 && now_time - on_time > TIME_OUT) {
			ESP_LOGI(TAG, "time out, off");
			off_all();
			on_gpio = -1;
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
