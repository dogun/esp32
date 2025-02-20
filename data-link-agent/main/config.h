/*
 * config.h
 *
 *  Created on: 2023��1��26��
 *      Author: yuexu
 */

#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

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
#define SERVER_PATH "/dl.php"

#define DATA_PASSWORD "1024"

char data[1024] = {0};


// 判断字符是否需要编码
int should_encode(char c) {
    return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~');
}

// URL编码函数
void url_encode(const char* input, int input_len, char* output, int output_len) {
    if (input == 0) return;
    if (output == 0) return;

    int j = 0;
    for (int i = 0; i < input_len; i++) {
		if (j >= output_len - 3) break;
        if (should_encode(input[i])) {
            sprintf(&output[j], "%%%02X", (unsigned char)input[i]);
            j += 3;
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
}

void _url_decode(char* res) {
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
}

#endif /* MAIN_CONFIG_H_ */
