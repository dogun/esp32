
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
#define CONFIG_SSID_LEN 32
#define CONFIG_PASS_LEN 64

static httpd_handle_t server;
static void (*_password_handler)(char*, char*);

void _url_decode(char* res) {
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
				if (isxdigit((int)res[i + 1]) && isxdigit((int)res[i + 2])) {
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

esp_err_t _get_handler_wifi(httpd_req_t* req) {
	char* res = "<html><body><form method=\"post\" action=\"/save\"><input name=\"ssid\" /><input name=\"pass\" /><input type=\"submit\" /></body></html>";
	httpd_resp_send(req, res, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

static char _ssid[CONFIG_SSID_LEN];
static char _pass[CONFIG_PASS_LEN];

esp_err_t _post_handler_save(httpd_req_t* req) {
	char content[1024] = {0};
	int recv_size = MIN(req->content_len, sizeof(content));
	int ret = httpd_req_recv(req, content, recv_size);
	if (ret <= 0) {
		if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
			httpd_resp_send_408(req);
		}
		return ESP_FAIL;
	}

	char* ssid = content;
	char* pass = strstr(content, "&");
	if (NULL == pass) {
		ESP_LOGI(HTTP_TAG, "pass not found");
		return ESP_FAIL;
	}
	*pass = 0;
	pass += 1;

	ESP_LOGI(HTTP_TAG, "ssid: %s, pass: %s", ssid, pass);

	ssid = strstr(ssid, "=");
	if (NULL == ssid || strlen(ssid) > 30) {
		ESP_LOGI(HTTP_TAG, "ssid not found");
		return ESP_FAIL;
	}
	ssid += 1;
	_url_decode(ssid);

	pass = strstr(pass, "=");
	if (NULL == pass || strlen(pass) > 62) {
		ESP_LOGI(HTTP_TAG, "pass not found(2)");
		return ESP_FAIL;
	}
	pass += 1;
	_url_decode(pass);

	strcpy(_ssid, ssid);
	strcpy(_pass, pass);
	_password_handler(_ssid, _pass);

	char* resp = "OK";
	httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

esp_err_t _get_handler_leds(httpd_req_t* req) {
	char* res = "<html><head><title>LED Controller</title></head><body><p><iframe id=\"res\" src=\"about:blank\"></iframe></p><script src=\"http://yueyue.com/led_con.js\"></script></body></html>";
	httpd_resp_send(req, res, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

esp_err_t _get_handler_duty(httpd_req_t* req) {
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


httpd_uri_t _uri_get_wifi = {
		.uri = "/wifi",
		.method = HTTP_GET,
		.handler = _get_handler_wifi,
		.user_ctx = NULL
};

httpd_uri_t _uri_post_save = {
		.uri = "/save",
		.method = HTTP_POST,
		.handler = _post_handler_save,
		.user_ctx = NULL
};

httpd_uri_t _uri_get_leds = {
		.uri = "/leds",
		.method = HTTP_GET,
		.handler = _get_handler_leds,
		.user_ctx = NULL
};

httpd_uri_t _uri_get_duty = {
		.uri = "/duty",
		.method = HTTP_GET,
		.handler = _get_handler_duty,
		.user_ctx = NULL
};

httpd_handle_t start_webserver(void (*h)(char*, char*)) {
	_password_handler = h;

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	if (httpd_start(&server, &config) == ESP_OK) {
		httpd_register_uri_handler(server, &_uri_get_wifi);
		httpd_register_uri_handler(server, &_uri_post_save);
		httpd_register_uri_handler(server, &_uri_get_leds);
		httpd_register_uri_handler(server, &_uri_get_duty);
	}
	return server;
}

void stop_webserver() {
	if (NULL != server) {
		httpd_stop(server);
	}
	server = NULL;
}

#endif /* MAIN_HTTP_OP_H_ */
