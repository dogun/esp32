
#ifndef MAIN_HTTP_OP_H_
#define MAIN_HTTP_OP_H_

#include <ctype.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include <esp_http_server.h>
#include "led_i2c.h"

#define HTTP_TAG "http-op"

void url_decode(char* res) {
	ESP_LOGI(HTTP_TAG, "urldecode res: %s", res);
	int d = 0; /* whether or not the string is decoded */

	char e_str[] = "00"; /* for a hex code */

	while (!d) {
		d = 1;
		int i; /* the counter for the string */

		for (i = 0; i < strlen(res); ++i) {
			if (res[i] == '%') {
				if (res[i + 1] == 0)
					return;
				if (isxdigit(res[i + 1]) && isxdigit(res[i + 2])) {
					d = 0;
					e_str[0] = res[i + 1];
					e_str[1] = res[i + 2];
					long int x = strtol(e_str, NULL, 16);
					memmove(&res[i + 1], &res[i + 3], strlen(&res[i + 3]) + 1);
					res[i] = x;
				}
			} else if (res[i] == '+') {
				res[i] = ' ';
			}
		}
	}
	ESP_LOGI(HTTP_TAG, "urldecode res ok: %s", res);
}

esp_err_t get_handler_duty(httpd_req_t* req) {
	char* data = strstr(req->uri, "=");

	if (NULL == data) {
		httpd_resp_send_404(req);
		return ESP_FAIL;
	}
	data += 1;

	uint8_t board_index = 0;
	uint8_t pin_id = 0;
	uint8_t duty = 0;

	char* tk;
	char* bbf = data;
	int i = 0;
	while ((tk = strsep(&bbf, ",")) != NULL) {
		if (strlen(tk) == 0)
			continue;
		if (i == 0)
			board_index = (uint8_t)atoi(tk);
		else if (i == 1)
			pin_id = (uint8_t)atoi(tk);
		else if (i == 2)
			duty = (uint8_t)atoi(tk);
		i++;
	}
	int ret = set_duty(board_index, pin_id, duty);

	char* res_ok = "OK";
	char* res_err = "ERR";

	char* res = res_ok;
	if (ret < 0) res = res_err;
	httpd_resp_send(req, res, strlen(res));
	return ESP_OK;
}

httpd_uri_t uri_get_duty = {
		.uri = "/duty",
		.method = HTTP_GET,
		.handler = get_handler_duty,
		.user_ctx = NULL
};

httpd_handle_t start_webserver(void) {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	httpd_handle_t server = NULL;

	if (httpd_start(&server, &config) == ESP_OK) {
		httpd_register_uri_handler(server, &uri_get_duty);
	}
	return server;
}

void stop_webserver(httpd_handle_t server) {
	if (server) {
		httpd_stop(server);
	}
}

#endif /* MAIN_HTTP_OP_H_ */
