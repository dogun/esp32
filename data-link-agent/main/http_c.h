/*
 * http_c.h
 *
 *  Created on: Jan 29, 2023
 *      Author: yuexu
 */

#ifndef MAIN_HTTP_C_H_
#define MAIN_HTTP_C_H_

#include "config.h"
#include <esp_http_client.h>

#define HTTP_C_TAG "http_c"

esp_err_t _http_c_event_handler(esp_http_client_event_t *evt) {
	return ESP_OK;
}

void set_data(char* data, size_t len) {
	char local_response_buffer[512] = {0};
	char q[128] = {0};
	sprintf(q, "", uid, tvoc, co2, jq);
    esp_http_client_config_t config = {
        .host = SERVER_HOST,
        .path = SERVER_PATH,
        .query = q,
        .event_handler = _http_c_event_handler,
        .user_data = local_response_buffer,        // Pass address of local buffer to get response
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    ESP_LOGI(HTTP_C_TAG, "host: %s, path: %s, query: %s", SERVER_HOST, SERVER_PATH, q);
    if (err == ESP_OK) {
        ESP_LOGI(HTTP_C_TAG, "HTTP GET Status = %d",
                esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(HTTP_C_TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    ESP_LOGI(HTTP_C_TAG, "set_data ok");
}

#endif /* MAIN_HTTP_C_H_ */
