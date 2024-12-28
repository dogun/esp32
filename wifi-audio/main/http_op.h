/*
 * http_op.h
 *
 *  Created on: 2023��1��26��
 *      Author: yuexu
 */

#ifndef MAIN_HTTP_OP_H_
#define MAIN_HTTP_OP_H_

#include "config.h"
#include "esp_err.h"
#include "esp_log.h"
#include <ctype.h>
#include <esp_http_server.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include "config_store.h"

#define H_TAG "http_op"

#define CONFIG_SIZE 600

void _url_decode(char *res) {
  ESP_LOGI(H_TAG, "urldecode res: %s", res);
  int d = 0; /* whether or not the string is decoded */

  char e_str[] = "00"; /* for a hex code */

  while (!d) {
    d = 1;
    unsigned int i; /* the counter for the string */

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
  ESP_LOGI(H_TAG, "urldecode res ok: %s", res);
}

static httpd_handle_t server;
static void (*_password_handler)(char *, char *, char *);

esp_err_t _get_handler_index(httpd_req_t *req) {
  char *res =
      "<html><body><form method=\"post\" action=\"/save\">ssid:<input "
      "name=\"ssid\" /><br/>pass:<input name=\"pass\" /><br/>uuid:<input "
      "name=\"uid\" /><input type=\"submit\" /></body></html>";
  httpd_resp_send(req, res, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static char _ssid[CONFIG_SSID_LEN];
static char _pass[CONFIG_PASS_LEN];
static char _uid[CONFIG_SSID_LEN];

esp_err_t _post_handler_save(httpd_req_t *req) {
  char content[1024] = {0};
  int recv_size = MIN(req->content_len, sizeof(content));
  int ret = httpd_req_recv(req, content, recv_size);
  if (ret <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  char *ssid = content;
  char *pass = strstr(content, "&");
  char *uid_p = pass + 1;
  char *uid = strstr(uid_p, "&");
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

  char *resp = "OK";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t get_handler_edit(httpd_req_t *req) {
  char *uri = strstr(req->uri, "=");

  if (NULL == uri) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  uri += 1;

  char config[CONFIG_SIZE] = {0};
  read_config(uri, config, sizeof(config));
  char *html = "<html><body><form method=\"post\"><input type=\"hidden\" "
               "name=\"k\" value=\"%s\" /><textarea name=\"eq\" "
               "style=\"width:300px;height:600px\">%s</textarea><input "
               "type=\"submit\" value=\"OK\" /></form></body></html>";
  char data[CONFIG_SIZE + 214] = {0};
  ESP_LOGI(H_TAG, "config: %s (%d), uri: %s (%d)", config, strlen(config),
           uri, strlen(uri));
  sprintf(data, html, uri, config);
  ESP_LOGI(H_TAG, "data: %s (%d)", data, strlen(data));
  httpd_resp_send(req, data, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t post_handler_edit(httpd_req_t *req) {
  char content[1024] = {0};
  int recv_size = MIN(req->content_len, sizeof(content));
  int ret = httpd_req_recv(req, content, recv_size);
  if (ret <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }
  _url_decode(content);

  char *token;
  char *cc = (char *)content;
  int i = 0;
  char *config_key = NULL;
  char *data = NULL;
  while ((token = strsep(&cc, "=")) != NULL) {
    if (i == 1)
      config_key = token;
    else if (i == 2)
      data = token;
    i++;
  }

  char *sp = strstr(config_key, "&");
  if (NULL != sp)
    sp[0] = 0;
  if (NULL == data)
    data = "";
  save_config(config_key, data);

  char *resp = "OK";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

httpd_uri_t _uri_get_index = {.uri = "/",
                              .method = HTTP_GET,
                              .handler = _get_handler_index,
                              .user_ctx = NULL};

httpd_uri_t _uri_post_save = {.uri = "/save",
                              .method = HTTP_POST,
                              .handler = _post_handler_save,
                              .user_ctx = NULL};

httpd_uri_t _uri_get_eq = {.uri = "/edit",
                           .method = HTTP_GET,
                           .handler = get_handler_edit,
                           .user_ctx = NULL};

httpd_uri_t _uri_post_eq = {.uri = "/edit",
                            .method = HTTP_POST,
                            .handler = post_handler_edit,
                            .user_ctx = NULL};

httpd_handle_t start_webserver(void (*h)(char *, char *, char *)) {
  _password_handler = h;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &_uri_get_index);
    httpd_register_uri_handler(server, &_uri_post_save);
    httpd_register_uri_handler(server, &_uri_get_eq);
    httpd_register_uri_handler(server, &_uri_post_eq);
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
