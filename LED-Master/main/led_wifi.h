/*
 * led_wifi.h
 *
 *  Created on: Sep 9, 2023
 *      Author: yuexu
 */

#ifndef MAIN_LED_WIFI_H_
#define MAIN_LED_WIFI_H_


#define SSID "LED-AP"
#define PASS "12345678"
#define MAX_CON 5

#define WIFI_TAG "wifi"

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data) {
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event =
				(wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(WIFI_TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac),
				event->aid);
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event =
				(wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(WIFI_TAG, "station "MACSTR" leave, AID=%d",
				MAC2STR(event->mac), event->aid);
	}
}

esp_netif_t* wifi_ap;
void wifi_init_softap(void) {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_ap = esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

	wifi_config_t wifi_config = { .ap = { .ssid = SSID, .ssid_len = strlen(
	SSID), .password = PASS, .max_connection =
	MAX_CON, .authmode = WIFI_AUTH_WPA_WPA2_PSK } };
	if (strlen(PASS) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
}

void wifi_deinit_softap(void) {
	esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
	ESP_ERROR_CHECK(esp_wifi_deinit());
	esp_netif_destroy_default_wifi(wifi_ap);
	ESP_ERROR_CHECK(esp_event_loop_delete_default());
	esp_netif_deinit();
}



#endif /* MAIN_LED_WIFI_H_ */
