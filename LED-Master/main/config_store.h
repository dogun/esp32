/*
 * config_store.h
 *
 *  Created on: 2022��5��20��
 *      Author: yuexq
 */

#ifndef MAIN_CONFIG_STORE_H_
#define MAIN_CONFIG_STORE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#define CONFIG_TAG "config-store"

#define STORAGE_NAMESPACE "storage"

void init_fs() {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES
			|| err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK (nvs_flash_erase());
		err = nvs_flash_init();
		ESP_LOGI(CONFIG_TAG, "erase nvs flash and init");
	}
	ESP_ERROR_CHECK(err);
}

nvs_handle_t _open_nvs() {
	nvs_handle_t my_handle;
	esp_err_t err;
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(CONFIG_TAG, "open nvs error:%s", esp_err_to_name(err));
		return 0;
	}
	return my_handle;
}

typedef unsigned int size_t;

void read_config(char* f, char* buf, int size) {
	nvs_handle_t my_handle = _open_nvs();

	size_t len = size;
	ESP_LOGI(CONFIG_TAG, "read data %s", f);
	esp_err_t err = nvs_get_str(my_handle, f, buf, &len);
	if (err != ESP_OK || len == 0) {
		ESP_LOGE(CONFIG_TAG, "get data error:%s, %s, %d", f, esp_err_to_name(err), len);
		nvs_close(my_handle);
		return;
	}
	nvs_close(my_handle);
}

void save_config(char* file, char* config) {
	nvs_handle_t my_handle = _open_nvs();
	ESP_LOGI(CONFIG_TAG, "write data %s %s", file, config);
	esp_err_t err = nvs_set_str(my_handle, file, config);
	if (err != ESP_OK) {
		ESP_LOGE(CONFIG_TAG, "set data error:%s, %d", file, err);
		nvs_close(my_handle);
		return;
	}
	err = nvs_commit(my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(CONFIG_TAG, "commit data error:%s, %d", file, err);
		nvs_close(my_handle);
		return;
	}
	nvs_close(my_handle);
}

#endif /* MAIN_CONFIG_STORE_H_ */
