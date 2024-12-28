/*
 * eq_config.h
 *
 *  Created on: 2022��5��17��
 *      Author: yuexq
 */

#ifndef MAIN_EQ_CONFIG_H_
#define MAIN_EQ_CONFIG_H_

#include <stdio.h>
#include "config.h"
#include "config_store.h"
#include "eq.h"

#define CONFIG_SIZE 600

#define CONFIG_KEY_L "leq"
#define CONFIG_KEY_R "req"

void _load_eq_new(char* buf, int ch) {
	char* tk;
	char* bbf = buf;
	while ((tk = strsep(&bbf, "\n")) != NULL) {
		if (strlen(tk) == 0)
			continue;

		if (!strstr(tk, "Fc") || !strstr(tk, "Gain") || !strstr(tk, "Q")) continue;

		char* token = strtok(tk, " ");
		double cf = 0, gain = 0, q = 0;
		int i = 0;
		while (token != NULL) {
			if (i == 5) cf = strtod(token, NULL);
			else if (i == 8) gain = strtod(token, NULL);
			else if (i == 11) q = strtod(token, NULL);
			i++;
			token = strtok(NULL, " ");
		}
		if (i == 1) { //只有一个数字
			//本來是設置delay的，現在不需要
		} else if(i > 11) {
			if (ch == L_CODE) {
				if (eq_len_l >= MAX_EQ_COUNT)
					continue;
				_mk_biquad(gain, cf, q, &(l_biquads[eq_len_l++]));
			} else {
				if (eq_len_r >= MAX_EQ_COUNT)
					continue;
				_mk_biquad(gain, cf, q, &(r_biquads[eq_len_r++]));
			}
		} else {
			ESP_LOGW(EQ_TAG, "_load_eq_new error: %s", tk);
		}
	}
}

void _load_eq_old(char* buf, int ch) {
	char* tk;
	char* bbf = buf;
	while ((tk = strsep(&bbf, "\n")) != NULL) {
		if (strlen(tk) == 0)
			continue;

		char* token;
		double cf = 0, gain = 0, q = 0;
		int i = 0;
		while ((token = strsep(&tk, " ")) != NULL) {
			if (i == 0)
				cf = strtod(token, NULL);
			else if (i == 1)
				gain = strtod(token, NULL);
			else if (i == 2)
				q = strtod(token, NULL);
			i++;
		}
		if (i == 1) { //只有一个数字
			//本來是設置delay，現在不需要
		} else if (i == 3) {
			if (ch == L_CODE) {
				if (eq_len_l >= MAX_EQ_COUNT)
					continue;
				_mk_biquad(gain, cf, q, &(l_biquads[eq_len_l++]));
			} else {
				if (eq_len_r >= MAX_EQ_COUNT)
					continue;
				_mk_biquad(gain, cf, q, &(r_biquads[eq_len_r++]));
			}
		} else {
			ESP_LOGW(EQ_TAG, "_load_eq_old error: %s", tk);
		}
	}
}

void _load_eq(int ch) {
	char buf[CONFIG_SIZE] = {0};
	if (ch == L_CODE) {
		read_config(CONFIG_KEY_L, buf, CONFIG_SIZE);
	} else {
		read_config(CONFIG_KEY_R, buf, CONFIG_SIZE);
	}

	if (strstr(buf, "Gain")) {
		_load_eq_new(buf, ch);
	} else {
		_load_eq_old(buf, ch);
	}
}

void load_eq() {
	_load_eq(L_CODE);
	_load_eq(R_CODE);
}

#endif /* MAIN_EQ_CONFIG_H_ */
