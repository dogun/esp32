/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <sys/unistd.h>
#include <time.h>
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "http_c.h"


#define TXD 5
#define RXD 4
#define RTS (UART_PIN_NO_CHANGE)
#define CTS (UART_PIN_NO_CHANGE)

#define UART_PORT_NUM      2
#define UART_BAUD_RATE     9600

#define TASK_STACK_SIZE    2048

static const char *TAG = "UART TVOC";

#define BUF_SIZE (1024)

static void tvoc_task(void *arg)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, TXD, RXD, RTS, CTS));

    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        if (len == 9) {
			co2 = (data[2] << 8) + data[3];
			tvoc = (data[4] << 8) + data[5];
			jq = (data[6] << 8) + data[7];
			ESP_LOGI(TAG, "Recv str:%d %d %d", tvoc, co2, jq);
        }
    }
}

void tvoc_run(void)
{
    xTaskCreate(tvoc_task, "uart_tvoc_task", TASK_STACK_SIZE, NULL, 10, NULL);
}
