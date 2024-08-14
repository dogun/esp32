/*
 * config.h
 *
 *  Created on: 2023��1��26��
 *      Author: yuexu
 */

#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#define CONFIG_SSID_LEN 32
#define CONFIG_PASS_LEN 64

#define CONFIG_SSID "ssid"
#define CONFIG_PASSWORD "password"
#define CONFIG_UID "uid"
#define CONFIG_STATUS "status"

#define ESP_WIFI_MAXIMUM_RETRY 5

#define AP_NAME "C-ESP-WIFI"
#define AP_PASSWORD "123456789"

#define SERVER_HOST "www.yueyue.com"
#define SERVER_PATH "/tvoc.php"

#define DATA_PASSWORD "1024"

//runtime config
int wifi_ready = 0;
int co2 = 0;
int tvoc = 0;
int jq = 0;

#endif /* MAIN_CONFIG_H_ */
