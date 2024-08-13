/*
 * config_store.h
 *
 *  Created on: 2022��5��20��
 *      Author: yuexq
 */

#ifndef MAIN_CONFIG_STORE_H_
#define MAIN_CONFIG_STORE_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <string.h>

#define CSTORE_TAG "cStore"
#define STORAGE_NAMESPACE "storage"

void init_fs() {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES
			|| err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK (nvs_flash_erase());
		err = nvs_flash_init();
		ESP_LOGI(CSTORE_TAG, "erase nvs flash and init");
	}
	ESP_ERROR_CHECK(err);
}

nvs_handle_t _open_nvs() {
	nvs_handle_t my_handle;
	esp_err_t err;
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(CSTORE_TAG, "open nvs error:%s", esp_err_to_name(err));
		return 0;
	}
	return my_handle;
}

typedef unsigned int size_t;

char* _get_len_key(char* f) {
	int key_len = strlen(f) + 5; //f_cnt\0
	char* key = (char*)calloc(key_len, sizeof(char));
	strcpy(key, f);
	strcat(key, "_cnt");
	return key;
}

int read_config_len(char* f) {
	nvs_handle_t my_handle = _open_nvs();

	char* key = _get_len_key(f);
	ESP_LOGI(CSTORE_TAG, "read data length %s", key);

	int32_t data_len = 0;
	esp_err_t err = nvs_get_i32(my_handle, key, &data_len);
	if (err != ESP_OK) {
		ESP_LOGE(CSTORE_TAG, "get data len error:%s, %s", key, esp_err_to_name(err));
	}
	nvs_close(my_handle);
	ESP_LOGI(CSTORE_TAG, "read data length %s : %d", key, (int)data_len);
	free(key);
	return data_len;
}

void read_config(char* f, char* buf, int size) {
	nvs_handle_t my_handle = _open_nvs();

	size_t len = size;
	ESP_LOGI(CSTORE_TAG, "read data %s", f);
	esp_err_t err = nvs_get_str(my_handle, f, buf, &len);
	if (err != ESP_OK || len == 0) {
		ESP_LOGE(CSTORE_TAG, "get data error:%s, %s, %d", f, esp_err_to_name(err), len);
		nvs_close(my_handle);
		return;
	}
	ESP_LOGI(CSTORE_TAG, "read data %s : %s", f, buf);
	nvs_close(my_handle);
}

void save_config(char* file, char* config) {
	nvs_handle_t my_handle = _open_nvs();
	ESP_LOGI(CSTORE_TAG, "write data %s %s", file, config);
	esp_err_t err = nvs_set_str(my_handle, file, config);
	if (err != ESP_OK) {
		ESP_LOGE(CSTORE_TAG, "set data error:%s, %d", file, err);
		nvs_close(my_handle);
		return;
	}
	char* key = _get_len_key(file);
	int len = strlen(config);
	ESP_LOGI(CSTORE_TAG, "write data len %s %d", file, len);
	nvs_set_i32(my_handle, key, len);

	err = nvs_commit(my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(CSTORE_TAG, "commit data error:%s, %d", key, err);
	}
	nvs_close(my_handle);
	free(key);
}

#endif /* MAIN_CONFIG_STORE_H_ */
