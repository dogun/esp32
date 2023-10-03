/*
 * V.h
 *
 *  Created on: Feb 14, 2023
 *      Author: yuexu
 */

#ifndef MAIN_V_H_
#define MAIN_V_H_

#include <driver/adc.h>
#include <esp_adc/adc_oneshot.h>

void init_adc1() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);
}

int adc1_get_val() {
    int val = adc1_get_raw(ADC1_CHANNEL_0);
    return val;
}


#endif /* MAIN_V_H_ */
