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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
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
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

PCD_HandleTypeDef hpcd_USB_FS;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_USB_PCD_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define SERVER

#define SERVER_ADDR_H 0x01
#define SERVER_ADDR_L 0x01

#define SERVER_CHANNEL_NO 0x2E
#define ME_CHANNEL_NO 0x2E

#ifdef SERVER
	#define ME_ADDR_H 0x01
	#define ME_ADDR_L 0x01
#else  //客户端的ID这里设置
	#define ME_ADDR_H 0x02
	#define ME_ADDR_L 0x02
#endif

#define HEAD_LEN 8
#define RECV_HEAD_LEN 5
#define PACKAGE_LEN 54
#define MAX_DATA_LEN PACKAGE_LEN - HEAD_LEN

typedef enum {
	DATA_TYPE_PT100_FLOW = 0x00U, DATA_TYPE_CURRENT = 0x01U
} data_type;

typedef struct _DATA_PACK {
	uint8_t addr_h;
	uint8_t addr_l;
	uint8_t type; //数据类型
	uint8_t crc;
	uint8_t len;
	uint8_t data[MAX_DATA_LEN];
} data_pack;

typedef struct _W_PACK {
	uint8_t addr_h;
	uint8_t addr_l;
	uint8_t channel;
	data_pack dp;
} w_pack;

uint8_t setting[6] = { 0 };
uint8_t setting1[6] = { 0 };
uint8_t setting2[6] = { 0 };

uint32_t timout_cnt = 0;
uint32_t crc_cnt = 0;
uint32_t size_cnt = 0;
uint32_t error_cnt = 0;
uint32_t ok_cnt1 = 0;
uint32_t ok_cnt2 = 0;

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

void wait_w() {
	while (aux_ready() != 1) {
		//nothing to do
	}
}

void check_recv() {
	while (aux_ready() != 0) {
		//nothing to do
	}
}

void log_error(HAL_StatusTypeDef e) {
	if (e == HAL_TIMEOUT)
		timout_cnt++;
	if (e == HAL_ERROR)
		error_cnt++;
}

void log_ok(data_type type) {
	if (type == DATA_TYPE_CURRENT) {
		ok_cnt2++;
	} else {
		ok_cnt1++;
	}
}

/**
 * 设置无线芯片参数，使用定点模式
 */
HAL_StatusTypeDef set_p() {
	wait_w();
	//设置模式
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); //M0
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET); //M1
	HAL_Delay(10); //等10ms
	//设置地址和信道等
	setting[0] = 0xC0;
	if (SERVER_ADDR_H == ME_ADDR_H && SERVER_ADDR_L == ME_ADDR_L) {
		setting[1] = SERVER_ADDR_H; //地址高字节
		setting[2] = SERVER_ADDR_L; //地址低字节
		setting[4] = SERVER_CHANNEL_NO; //信道
	} else {
		setting[1] = ME_ADDR_H;
		setting[2] = ME_ADDR_L;
		setting[4] = ME_CHANNEL_NO;
	}
	setting[3] = 0x1D; //波特率等 空中速率50K
	setting[5] = 0x80; //单点模式
	__HAL_UART_FLUSH_DRREGISTER(&huart3);
	HAL_StatusTypeDef ret = HAL_UART_Transmit(&huart3, setting, sizeof(setting),
			1000);
	if (ret != HAL_OK) {
		log_error(ret);
		return ret;
	}
	ret = HAL_UART_Receive(&huart3, setting1, sizeof(setting1), 1000);
	if (ret != HAL_OK) {
		log_error(ret);
		return ret;
	}
	HAL_Delay(10); //等10ms
	//查询设置
	uint8_t query[3] = { 0xC1, 0xC1, 0xC1 };
	ret = HAL_UART_Transmit(&huart3, query, sizeof(query), 1000);
	if (ret != HAL_OK) {
		log_error(ret);
		return ret;
	}
	ret = HAL_UART_Receive(&huart3, setting2, sizeof(setting2), 1000);
	if (ret != HAL_OK) {
		log_error(ret);
		return ret;
	}
	//发射模式
	wait_w();
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET); //M0
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET); //M1
	return HAL_OK;
}

//永远都发送package size
HAL_StatusTypeDef send_data(uint8_t addrh, uint8_t addrl, uint8_t ch,
		data_type type, uint8_t *data, size_t len) {
	w_pack wp = { 0 };
	wp.addr_h = addrh;
	wp.addr_l = addrl;
	wp.channel = ch;
	wp.dp.addr_h = ME_ADDR_H;
	wp.dp.addr_l = ME_ADDR_L;
	wp.dp.type = type;
	wp.dp.len = len;
	memcpy(wp.dp.data, data, len);
	wp.dp.crc = crc8(wp.dp.data, len);
	wait_w();
	HAL_StatusTypeDef ret = HAL_UART_Transmit(&huart3, (uint8_t*) &wp,
	HEAD_LEN + len, 1000);
	if (ret != HAL_OK) {
		log_error(ret);
	}
	log_ok(type);
	return ret;
}

HAL_StatusTypeDef send_data_to_server(data_type type, uint8_t *data, size_t len) {
	return send_data(SERVER_ADDR_H, SERVER_ADDR_L, SERVER_CHANNEL_NO, type,
			data, len);
}

/*
 //透明传输模式
 HAL_StatusTypeDef send_data(data_pack* dp) {
 wait_w();
 __HAL_UART_FLUSH_DRREGISTER(&huart3);
 return HAL_UART_Transmit(&huart3, (uint8_t*)dp, RECV_HEAD_LEN + dp->len, 1000);
 }
 */

HAL_StatusTypeDef recv_data(data_pack *dp) {
	//check_recv();
	//__HAL_UART_FLUSH_DRREGISTER(&huart3);
	HAL_StatusTypeDef ret = HAL_UART_Receive(&huart3, (uint8_t*) dp,
	RECV_HEAD_LEN, 2000);
	if (ret != HAL_OK) {
		log_error(ret);
		return ret;
	}
	if (dp->len > MAX_DATA_LEN) { //肯定接受错误
		size_cnt++;
		return HAL_ERROR;
	}
	ret = HAL_UART_Receive(&huart3, (uint8_t*) dp->data, dp->len, 2000);
	int crc = crc8(dp->data, dp->len);
	if (crc != dp->crc) {
		crc_cnt++;
		return HAL_ERROR;
	}
	log_ok(dp->type);
	return HAL_OK;
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

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
	while (1) {
		if (SERVER_ADDR_H != ME_ADDR_H || SERVER_ADDR_L != ME_ADDR_L) {
			HAL_Delay(1000);

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
			if (ret != HAL_OK) {
				continue;
			}

			HAL_Delay(1000);
			//3. current数据
			data[0] = val[12];
			data[1] = val[13];
			data[2] = val[14];
			data[3] = val[15];
			ret = send_data_to_server(DATA_TYPE_CURRENT, (uint8_t*) data,
					4 * sizeof(uint32_t));

			if (ret != HAL_OK) {
				continue;
			}
		} else {
			data_pack dp = { 0 };
			HAL_StatusTypeDef r = recv_data(&dp);
			if (r != HAL_OK) {
				continue;
			}
			if (dp.type == DATA_TYPE_PT100_FLOW) {
				dp.crc = 1;
			} else if (dp.type == DATA_TYPE_CURRENT) {
				dp.crc = 2;
			}
		}
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

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
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC | RCC_PERIPHCLK_USB;
	PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
	PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */

	/** Common config
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion = 1;
	if (HAL_ADC_Init(&hadc1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */

	/* USER CODE END ADC1_Init 2 */

}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 0;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 65535;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */
	HAL_TIM_MspPostInit(&htim2);

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 0;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 65535;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */
	HAL_TIM_MspPostInit(&htim3);

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 9600;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void) {

	/* USER CODE BEGIN USART3_Init 0 */

	/* USER CODE END USART3_Init 0 */

	/* USER CODE BEGIN USART3_Init 1 */

	/* USER CODE END USART3_Init 1 */
	huart3.Instance = USART3;
	huart3.Init.BaudRate = 9600;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART3_Init 2 */

	/* USER CODE END USART3_Init 2 */

}

/**
 * @brief USB Initialization Function
 * @param None
 * @retval None
 */
static void MX_USB_PCD_Init(void) {

	/* USER CODE BEGIN USB_Init 0 */

	/* USER CODE END USB_Init 0 */

	/* USER CODE BEGIN USB_Init 1 */

	/* USER CODE END USB_Init 1 */
	hpcd_USB_FS.Instance = USB;
	hpcd_USB_FS.Init.dev_endpoints = 8;
	hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
	hpcd_USB_FS.Init.low_power_enable = DISABLE;
	hpcd_USB_FS.Init.lpm_enable = DISABLE;
	hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
	if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USB_Init 2 */

	/* USER CODE END USB_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9,
			GPIO_PIN_RESET);

	/*Configure GPIO pins : PC13 PC14 PC15 PC12 */
	GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PB2 PB12 PB13 PB14
	 PB15 */
	GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
			| GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : PC8 PC9 */
	GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : PA8 */
	GPIO_InitStruct.Pin = GPIO_PIN_8;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PD2 */
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pins : PB4 PB5 PB8 PB9 */
	GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
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
