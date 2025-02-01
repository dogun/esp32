/*
 * crc8.c
 *
 *  Created on: Jan 31, 2025
 *      Author: D
 */


#include "crc8.h"

uint8_t crc8(const uint8_t *data, size_t length) {
	uint8_t polynomial = 0x07;
	uint8_t initial_value = 0x00;

	uint8_t crc = initial_value;
	for (int i = 0; i < length; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ polynomial;
			} else {
				crc <<= 1;
			}
		}
	}
	return crc;
}
