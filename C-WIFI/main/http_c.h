/*
 * http_c.h
 *
 *  Created on: Jan 29, 2023
 *      Author: yuexu
 */

#ifndef MAIN_HTTP_C_H_
#define MAIN_HTTP_C_H_

#include <esp_http_client.h>

#define HTTP_C_TAG "http_c"

esp_err_t _http_c_event_handler(esp_http_client_event_t *evt) {
	return ESP_OK;
}

void set_data(int data) {
	char local_response_buffer[512] = {0};
	char q[128] = {0};
	sprintf(q, "p=%s&t=1&n=%d", DATA_PASSWORD, data);
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
    if (err == ESP_OK) {
        ESP_LOGI(HTTP_C_TAG, "HTTP GET Status = %d, content_length = %"PRIu64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(HTTP_C_TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(HTTP_C_TAG, "HTTP res: %s, len: %d", local_response_buffer, strlen(local_response_buffer));

    esp_http_client_cleanup(client);
}

#endif /* MAIN_HTTP_C_H_ */
