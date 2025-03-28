/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "usb.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "crc8.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * 采用透传模式，共支持10个客户端，全部采用FF信道，但是定义各自的ID，互相协同，按照顺序分别发送
 */

#define SERVER
#define CLIENT_ID 0x02
#define CLIENT_NUM 3
#define SEND_WAIT_TIME 2000

#define S_1 0x10
#define S_2 0x24

#define ADDR_H 0xFF
#define ADDR_L 0xFF
#define CHANNEL_NO 0x2E

#define HEAD_LEN 6
#define PACKAGE_LEN 54
#define MAX_DATA_LEN PACKAGE_LEN - HEAD_LEN

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

typedef enum {
	DATA_TYPE_PT100_FLOW = 0x00U, DATA_TYPE_CURRENT = 0x01U
} data_type;

typedef struct _DATA_PACK {
	uint8_t s_1; //0x10
	uint8_t s_2; //0x24
	uint8_t client_id;
	uint8_t type; //数据类型
	uint8_t crc;
	uint8_t len;
	uint8_t data[MAX_DATA_LEN];
} data_pack;

typedef struct _DATA_LOGGER {
	uint32_t recv_s_error_cnt;
	uint32_t recv_cnt_by_client[10];
	uint32_t recv_ok_cnt_1;
	uint32_t recv_ok_cnt_2;
	uint32_t send_ok_cnt_1;
	uint32_t send_ok_cnt_2;
	uint32_t send_ok_cnt;
	uint32_t send_ok_timeout_cnt;
	uint32_t recv_timout_cnt;
	uint32_t send_timout_cnt;
	uint32_t recv_crc_error_cnt;
	uint32_t recv_size_error_cnt;
	uint32_t recv_client_error_cnt;
	uint32_t recv_unknow_error_cnt;
	uint32_t send_unknow_error_cnt;
	uint32_t uart1_error_cnt;

} data_logger;

data_pack recv_buf = { 0 };
data_pack send_buf = { 0 };
data_logger logger = { 0 };

uint32_t last_recv_time = 0;
uint32_t last_recv_time_a[CLIENT_NUM] = {0};
uint32_t last_send_time = 0;
uint8_t last_client_id = 0;

uint32_t get_adc_val(uint32_t index) {
	ADC_ChannelConfTypeDef sConfig = { 0 };
	switch (index) {
	case 0:
		sConfig.Channel = ADC_CHANNEL_0;
		break;
	case 1:
		sConfig.Channel = ADC_CHANNEL_1;
		break;
	case 2:
		sConfig.Channel = ADC_CHANNEL_2;
		break;
	case 3:
		sConfig.Channel = ADC_CHANNEL_3;
		break;
	case 4:
		sConfig.Channel = ADC_CHANNEL_4;
		break;
	case 5:
		sConfig.Channel = ADC_CHANNEL_5;
		break;
	case 6:
		sConfig.Channel = ADC_CHANNEL_6;
		break;
	case 7:
		sConfig.Channel = ADC_CHANNEL_7;
		break;
	case 8:
		sConfig.Channel = ADC_CHANNEL_8;
		break;
	case 9:
		sConfig.Channel = ADC_CHANNEL_9;
		break;
	case 10:
		sConfig.Channel = ADC_CHANNEL_10;
		break;
	case 11:
		sConfig.Channel = ADC_CHANNEL_11;
		break;
	case 12:
		sConfig.Channel = ADC_CHANNEL_12;
		break;
	case 13:
		sConfig.Channel = ADC_CHANNEL_13;
		break;
	case 14:
		sConfig.Channel = ADC_CHANNEL_14;
		break;
	case 15:
		sConfig.Channel = ADC_CHANNEL_15;
		break;

	default:
		break;
	}
	sConfig.Rank = 1;

	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
		Error_Handler();
	}

	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	uint32_t val = HAL_ADC_GetValue(&hadc1);

	return val;
}

uint32_t aux_ready() {
	GPIO_PinState s = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8);
	if (s == GPIO_PIN_RESET) {
		return 0;
	} else {
		return 1;
	}
}

void wait_w() { //等待可写
	while (aux_ready() != 1) {
		//nothing to do
	}
}

void recv_log_error(HAL_StatusTypeDef e) {
	if (e == HAL_TIMEOUT)
		logger.recv_timout_cnt++;
	if (e == HAL_ERROR)
		logger.recv_unknow_error_cnt++;
}

void send_log_error(HAL_StatusTypeDef e) {
	if (e == HAL_TIMEOUT)
		logger.send_timout_cnt++;
	if (e == HAL_ERROR)
		logger.send_unknow_error_cnt++;
}

void recv_log_ok(data_type type) {
	if (type == DATA_TYPE_CURRENT) {
		logger.recv_ok_cnt_2++;
	} else {
		logger.recv_ok_cnt_1++;
	}
}

void send_log_ok(data_type type) {
	if (type == DATA_TYPE_CURRENT) {
		logger.send_ok_cnt_2++;
	} else {
		logger.send_ok_cnt_1++;
	}
}

HAL_StatusTypeDef get_p() {
	//查询设置
	uint8_t query[3] = { 0xC1, 0xC1, 0xC1 };
	HAL_StatusTypeDef ret = HAL_UART_Transmit(&huart3, query, sizeof(query), 1000);
	if (ret != HAL_OK) {
		send_log_error(ret);
		return ret;
	}
	uint8_t setting[6] = { 0 };
	ret = HAL_UART_Receive(&huart3, setting, sizeof(setting), 1000);
	if (ret != HAL_OK) {
		recv_log_error(ret);
		return ret;
	}
	return HAL_OK;
}

/**
 * 设置无线芯片参数，使用定点模式
 */
uint8_t setup_f = 0;
HAL_StatusTypeDef set_p() {
	//wait_w();
	//设置模式
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); //M0
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET); //M1
	HAL_Delay(10); //等10ms
	//设置地址和信道等
	uint8_t setting[6] = { 0 };
	setting[0] = 0xC0;
	setting[1] = ADDR_H; //地址高字节
	setting[2] = ADDR_L; //地址低字节
	setting[4] = CHANNEL_NO; //信道
	setting[3] = 0x1D; //波特率等 空中速率50K
	setting[5] = 0x00; //透传模式
	__HAL_UART_FLUSH_DRREGISTER(&huart3);
	HAL_StatusTypeDef ret = HAL_UART_Transmit(&huart3, setting, sizeof(setting),
			1000);
	if (ret != HAL_OK) {
		send_log_error(ret);
		return ret;
	}
	HAL_UART_Receive(&huart3, setting, sizeof(setting), 1000); //接收返回，不做校验了
	HAL_Delay(10); //等10ms
	//发射模式
	//wait_w();
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); //M0
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET); //M1
	setup_f = 1;
	return HAL_OK;
}

//永远都发送package size
HAL_StatusTypeDef send_data(uint8_t addrh, uint8_t addrl, uint8_t ch,
		data_type type, uint8_t *data, size_t len) {
	send_buf.s_1 = S_1;
	send_buf.s_2 = S_2;
	send_buf.client_id = CLIENT_ID;
	send_buf.type = type;
	send_buf.len = len;
	memcpy(send_buf.data, data, len);
	send_buf.crc = crc8(send_buf.data, len);
	if (send_buf.crc == 0) {
		send_buf.crc = 111;
	}
	//wait_w();
	HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(&huart3, (uint8_t*) &send_buf,
	PACKAGE_LEN);
	if (ret != HAL_OK) {
		send_log_error(ret);
	}
	send_log_ok(type);
	return ret;
}

HAL_StatusTypeDef send_data_to_server(data_type type, uint8_t *data, size_t len) {
	return send_data(ADDR_H, ADDR_L, CHANNEL_NO, type, data, len);
}

HAL_StatusTypeDef recv_data(uint8_t reset) {
	if (reset) __HAL_UART_FLUSH_DRREGISTER(&huart3);
	HAL_StatusTypeDef ret = HAL_UART_Receive_DMA(&huart3, (uint8_t*) &recv_buf,
	PACKAGE_LEN);
	if (ret != HAL_OK) {
		recv_log_error(ret);
		return ret;
	}
	return HAL_OK;
}

HAL_StatusTypeDef send_pt100_flow_data() {
	int val[16] = { 0 };
	uint32_t i = 0;
	for (i = 0; i < 16; ++i) {
		val[i] = get_adc_val(i);
	}

	//填充数据
	uint32_t data[8] = { 0 };

	//1.PT100数据
	data[0] = (val[0] > val[1]) ? val[0] - val[1] : val[1] - val[0];
	data[1] = (val[2] > val[3]) ? val[2] - val[3] : val[3] - val[2];
	data[2] = (val[4] > val[5]) ? val[4] - val[5] : val[5] - val[4];
	data[3] = (val[6] > val[7]) ? val[6] - val[7] : val[7] - val[6];
	//2.flow数据
	data[4] = val[8];
	data[5] = val[9];
	data[6] = val[10];
	data[7] = val[11];

	HAL_StatusTypeDef ret = send_data_to_server(DATA_TYPE_PT100_FLOW,
			(uint8_t*) data, 8 * sizeof(uint32_t));
	return ret;
}

HAL_StatusTypeDef send_current_data() {
	int val[16] = { 0 };
	uint32_t i = 0;
	for (i = 0; i < 16; ++i) {
		val[i] = get_adc_val(i);
	}
	//填充数据
	uint32_t data[4] = { 0 };

	//3. current数据
	data[0] = val[12];
	data[1] = val[13];
	data[2] = val[14];
	data[3] = val[15];
	HAL_StatusTypeDef ret = send_data_to_server(DATA_TYPE_CURRENT,
			(uint8_t*) data, 4 * sizeof(uint32_t));

	return ret;
}

uint8_t _switch = 0;
HAL_StatusTypeDef send_data_switch() {
	HAL_StatusTypeDef ret;
	if (_switch == 0) {
		ret = send_pt100_flow_data();
		_switch = 1;
	} else {
		ret = send_current_data();
		_switch = 0;
	}
	return ret;
}

uint8_t pre_id(uint8_t me) {
	if (me == 1) return CLIENT_NUM;
	return me - 1;
}

uint8_t is_pre(uint8_t me, uint8_t pre) {
	if (pre == 0) return 0;
	return pre == pre_id(me);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART3_UART_Init();
  MX_I2C1_Init();
  MX_USB_PCD_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	HAL_Delay(1000);
	while (set_p() != HAL_OK) {
		HAL_Delay(100);
	}

	last_recv_time = HAL_GetTick();
	last_recv_time_a[pre_id(CLIENT_ID)] = last_recv_time;
	recv_data(0);
	while (1) {
#ifdef SERVER
		//不需要做事情了
#else
		//一直循环计时，判断之前收到的消息id是不是自己的上一个，如果不是，继续等待接收，直到上一个到达
		uint32_t now = HAL_GetTick();
		if (is_pre(CLIENT_ID, last_client_id)) { //现在收到的是我的上一个
			if (last_send_time < last_recv_time_a[pre_id(CLIENT_ID) - 1]) { //是新收到的消息，这里不使用last_client_id，避免中间过程被中断改写
				send_data_switch();
				logger.send_ok_cnt ++;
				last_send_time = now;
			}
		} else {
			//不是上一个，则计算上一次收到的id和我的距离
			uint32_t dist =
					(last_client_id > CLIENT_ID) ?
					CLIENT_NUM - last_client_id + CLIENT_ID :
													CLIENT_ID - last_client_id;
			if (now - MAX(last_recv_time, last_send_time) >= SEND_WAIT_TIME * dist) { //需要发送了
				send_data_switch();
				logger.send_ok_timeout_cnt++;
				last_send_time = now;
			}
		}
#endif
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart) {

}

HAL_StatusTypeDef check_pack() {
	if (recv_buf.len > MAX_DATA_LEN) { //肯定接受错误
		logger.recv_size_error_cnt++;
		return HAL_ERROR;
	}
	if (recv_buf.s_1 != S_1 || recv_buf.s_2 != S_2) { //数据错乱
		logger.recv_s_error_cnt++;
		return HAL_ERROR;
	}
	int crc = crc8(recv_buf.data, recv_buf.len);
	if (!(crc == recv_buf.crc || (crc == 0 && recv_buf.crc == 111))) {
		logger.recv_crc_error_cnt++;
		return HAL_ERROR;
	}
	if (recv_buf.client_id < 1 || recv_buf.client_id > CLIENT_NUM) { //clientid 错误
		logger.recv_client_error_cnt++;
		return HAL_ERROR;
	}
	return HAL_OK;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart) {
	if (check_pack() != HAL_OK) {
		recv_data(1); //继续接收
		return;
	}
	recv_log_ok(recv_buf.type);
#ifdef SERVER
	logger.recv_cnt_by_client[recv_buf.client_id-1]++;
	memcpy((void*)&send_buf, (void*)&recv_buf, PACKAGE_LEN);
	recv_data(0);
	last_recv_time = HAL_GetTick();
	HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(&huart1, (uint8_t*)&send_buf, PACKAGE_LEN);
	if (ret != HAL_OK) {
		logger.uart1_error_cnt++;
	}
#else
	last_client_id = recv_buf.client_id;
	recv_data(0); //继续接收
	logger.recv_cnt_by_client[last_client_id-1]++;
	last_recv_time = HAL_GetTick();
	last_recv_time_a[last_client_id - 1] = last_recv_time;
#endif
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uh) {
	recv_data(1);
	logger.recv_unknow_error_cnt ++;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
