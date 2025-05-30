/*
 * http_op.h
 *
 *  Created on: 2023��1��26��
 *      Author: yuexu
 */

#ifndef MAIN_HTTP_OP_H_
#define MAIN_HTTP_OP_H_

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <esp_http_server.h>
#include <ctype.h>
#include "config.h"
#include "esp_err.h"

#define H_TAG "http_op"

static httpd_handle_t server;
static void (*_password_handler)(char*, char*, char*);

esp_err_t _get_handler_index(httpd_req_t* req) {
	char* res = "<html><body><form method=\"post\" action=\"/save\"><input name=\"ssid\" /><br/><input name=\"pass\" /><br/><input name=\"uid\" /><input type=\"submit\" /></body></html>";
	httpd_resp_send(req, res, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

static char _ssid[CONFIG_SSID_LEN];
static char _pass[CONFIG_PASS_LEN];
static char _uid[CONFIG_SSID_LEN];

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
	char* uid_p = pass + 1;
	char* uid = strstr(uid_p, "&");
	if (NULL == pass) {
		ESP_LOGI(H_TAG, "pass not found");
		return ESP_FAIL;
	}
	if (NULL == uid) {
		ESP_LOGI(H_TAG, "uid not found");
		return ESP_FAIL;
	}
	*pass = 0;
	pass += 1;
	*uid = 0;
	uid += 1;

	ESP_LOGI(H_TAG, "ssid: %s, pass: %s, uid: %s", ssid, pass, uid);

	ssid = strstr(ssid, "=");
	if (NULL == ssid || strlen(ssid) > CONFIG_SSID_LEN) {
		ESP_LOGI(H_TAG, "ssid not found");
		return ESP_FAIL;
	}
	ssid += 1;
	_url_decode(ssid);

	pass = strstr(pass, "=");
	if (NULL == pass || strlen(pass) > CONFIG_PASS_LEN) {
		ESP_LOGI(H_TAG, "pass not found(2)");
		return ESP_FAIL;
	}
	pass += 1;
	_url_decode(pass);

	uid = strstr(uid, "=");
	if (NULL == uid || strlen(uid) > CONFIG_SSID_LEN) {
		ESP_LOGE(H_TAG, "uid not found");
		return ESP_FAIL;
	}
	uid += 1;
	_url_decode(uid);

	strcpy(_ssid, ssid);
	strcpy(_pass, pass);
	strcpy(_uid, uid);
	_password_handler(_ssid, _pass, _uid);

	char* resp = "OK";
	httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

httpd_uri_t _uri_get_index = {
		.uri = "/",
		.method = HTTP_GET,
		.handler = _get_handler_index,
		.user_ctx = NULL
};

httpd_uri_t _uri_post_save = {
		.uri = "/save",
		.method = HTTP_POST,
		.handler = _post_handler_save,
		.user_ctx = NULL
};


httpd_handle_t start_webserver(void (*h)(char*, char*, char*)) {
	_password_handler = h;

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	if (httpd_start(&server, &config) == ESP_OK) {
		httpd_register_uri_handler(server, &_uri_get_index);
		httpd_register_uri_handler(server, &_uri_post_save);
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
