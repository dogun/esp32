/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/*
 * 协议：
 * PWD:SERV:NUM:OPEN. 开NUM对应的锁
 * PWD:SERV:NUM:CLOSE. 关NUM对应的锁
 * PWD:SYS:0:VOL. 获取电压
 * RES:SUCCESS. 操作成功
 * RES:FAIL:xxx. 操作失败，xxx为错误原因
 * VOL:3900. 返回电压
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
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

typedef struct _REQ {
	char pwd[10];
	char type[6];
	char num[6];
	char action[10];
} _req;

void parse_req(char *req, _req *data) {
	if (NULL == req || strlen(req) == 0)
		return;

	bzero(data->pwd, sizeof(data->pwd));
	bzero(data->type, sizeof(data->type));
	bzero(data->num, sizeof(data->num));
	bzero(data->action, sizeof(data->action));

	char *cc = req;
	int i = 0;
	char *token;
	while ((token = strsep(&cc, ":")) != NULL) {
		if (i == 0) {
			if (strlen(token) < sizeof(data->pwd)) {
				strcpy(data->pwd, token);
			}
		} else if (i == 1) {
			if (strlen(token) < sizeof(data->type)) {
				strcpy(data->type, token);
			}
		} else if (i == 2) {
			if (strlen(token) < sizeof(data->num)) {
				strcpy(data->num, token);
			}
		} else if (i == 3) {
			if (strlen(token) < sizeof(data->action)) {
				strcpy(data->action, token);
			}
		} else {
			return;
		}
		i++;
	}
}

uint32_t set_serv_angle(uint32_t channel, int16_t angle) { //20ms 对应 10000 跳， 1ms 500 跳，0度1ms，180度2ms
	//计算脉冲
	if (angle < 0)
		angle = 0;
	if (angle > 180)
		angle = 180;
	uint32_t pulse = 500 + (angle * 500) / 180;

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); //舵机上电
	HAL_Delay(50); //等上电
	HAL_TIM_PWM_Start(&htim2, channel); //开始输出舵机信号
	HAL_Delay(50);
	__HAL_TIM_SET_COMPARE(&htim2, channel, pulse);
	HAL_Delay(500); //等舵机旋转
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); //舵机下电
	HAL_TIM_PWM_Stop(&htim2, channel); //关闭舵机信号

	return pulse;
}

void send_data(char *str) {
	HAL_UART_Transmit(&huart2, (uint8_t*) str, strlen(str), 1000);
}

uint32_t get_bat_vol() { //满量程 3.3/0.6
	HAL_ADC_Start(&hadc);
	HAL_ADC_PollForConversion(&hadc, 200);
	uint32_t vol = HAL_ADC_GetValue(&hadc);
	HAL_ADC_Stop(&hadc);
	return ((100 * vol / 4095) * 330 / 6);
}

int opened = 0;
void process(char* data_buff) {
	if (strlen(data_buff) < 4) {
		send_data("RES:FAIL:NOREQ.");
	}
	_req req_data;
	parse_req(data_buff, &req_data);
	if (strcmp(req_data.pwd, "0123") != 0) {
		send_data("RES:FAIL:PWD.");
		return;
	}
	if (strcmp(req_data.type, "SERV") == 0) {
		uint32_t TIM_CHANNEL_X = 0;
		if (strcmp(req_data.num, "1") == 0) {
			TIM_CHANNEL_X = TIM_CHANNEL_1;
		} else if (strcmp(req_data.num, "2") == 0) {
			TIM_CHANNEL_X = TIM_CHANNEL_2;
		} else if (strcmp(req_data.num, "3") == 0) {
			TIM_CHANNEL_X = TIM_CHANNEL_3;
		} else if (strcmp(req_data.num, "4") == 0) {
			TIM_CHANNEL_X = TIM_CHANNEL_4;
		} else {
			send_data("RES:FAIL:NUM.");
			return;
		}
		if (strcmp(req_data.action, "OPEN") == 0) {
			set_serv_angle(TIM_CHANNEL_X, 180);
			opened = 1;
		} else if(strcmp(req_data.action, "CLOSE") == 0) {
			set_serv_angle(TIM_CHANNEL_X, 0);
		} else {
			send_data("RES:FAIL:ACTION.");
			return;
		}
		send_data("RES:SUCCESS.");
	} else if (strcmp(req_data.type, "SYS") == 0) {
		if (strcmp(req_data.action, "VOL") == 0) {
			uint32_t v = get_bat_vol();
			char v_buf[32] = {0};
			sprintf(v_buf, "VOL:%d.", (int)v);
			send_data(v_buf);
		}
	} else {
		send_data("RES:FAIL:TYPE.");
		return;
	}
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
	MX_USART2_UART_Init();
	MX_TIM2_Init();
	MX_ADC_Init();
	/* USER CODE BEGIN 2 */
	HAL_Delay(1000);
	set_serv_angle(TIM_CHANNEL_1, 0);
	set_serv_angle(TIM_CHANNEL_2, 0);
	set_serv_angle(TIM_CHANNEL_3, 0);
	set_serv_angle(TIM_CHANNEL_4, 0);

	char data_buff[40] = { 0 };
	int index = 0;
	char rx_buff[1] = {0};
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		if (opened > 0) { //门打开状态
			opened++;
			if (opened > 60) { //打开太久了
				set_serv_angle(TIM_CHANNEL_1, 0);
				set_serv_angle(TIM_CHANNEL_2, 0);
				set_serv_angle(TIM_CHANNEL_3, 0);
				opened = 0;
			}
		}
		if (HAL_UART_Receive(&huart2, (uint8_t*)rx_buff, sizeof(rx_buff), 1000) == HAL_OK) {
			if (rx_buff[0] != '.' && index < sizeof(data_buff)) {
				data_buff[index++] = rx_buff[0];
			} else if (rx_buff[0] == '.') {
				process(data_buff);
				bzero(data_buff, sizeof(data_buff));
				index = 0;
			} else {
				send_data("RES:FAIL:LEN.");
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

	/** Configure the main internal regulator output voltage
	 */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_DIV4;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
	RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV4;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
	PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		Error_Handler();
	}
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
