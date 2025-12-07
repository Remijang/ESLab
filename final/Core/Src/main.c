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

#include "cmsis_os.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "arm_math.h"
#include "rnn.h"
#include "stdio.h"
#include "usbd_hid.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define timerDelay 300U
#define NUM_SAMPLE 18

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

QSPI_HandleTypeDef hqspi;

SPI_HandleTypeDef hspi3;

TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
osThreadId_t tid_send;
const osThreadAttr_t TaskSend_attributes = {
	.name = "TaskSend",
	.stack_size = 256 * 4,
	.priority = (osPriority_t)osPriorityLow,
};
osThreadId_t tid_send_rnn;
const osThreadAttr_t TaskSendRNN_attributes = {
	.name = "TaskRNN",
	.stack_size = 256 * 4,
	.priority = (osPriority_t)osPriorityLow,
};

int16_t pDataXYZ[3];
report_t report;
uint32_t freq = 100;

extern USBD_HandleTypeDef hUsbDeviceFS;
extern const float rnn_alpha;

float32_t DtD_data[36];
float32_t Dt1_data[6];

arm_matrix_instance_f32 DtD;
arm_matrix_instance_f32 Dt1;

float32_t DtD_inv_data[36];
arm_matrix_instance_f32 DtD_inv;

float32_t v_data[6];
arm_matrix_instance_f32 v;

float32_t sample_data[NUM_SAMPLE][3] = {
	{-13, 14, 1010},  {-1009, -14, 9},	{985, -21, 64},	   {-272, -273, 937},
	{814, -302, 502}, {-51, -20, -996}, {-317, 174, -932}, {-352, -41, 944},
	{4, 354, 937},	  {-5, -555, 855},	{-7, -803, 634},   {-8, 607, 787},
	{682, -22, 737},  {-520, -60, 861}, {-578, -57, -825}, {357, -36, 918},
	{70, -451, -890}, {-6, 468, -872},
};

typedef struct {
	float offset[3];
	float gain[3];
} SensorCal_t;

SensorCal_t cal_data;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_QUADSPI_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM16_Init(void);
void Timer_CallBack(void *argument);

/* USER CODE BEGIN PFP */
void ACC_InitGPIO(void);
void Task_Send(void *argument);
void Task_Send_RNN(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
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
	MX_I2C2_Init();
	MX_QUADSPI_Init();
	MX_USART3_UART_Init();
	BSP_COM_Init(COM1);
	MX_TIM16_Init();
	/* USER CODE BEGIN 2 */
	ACCELERO_StatusTypeDef ret = BSP_ACCELERO_Init();
	if (ret != ACCELERO_OK) {
		printf("Error: %d\n", ret);
	}
	ACC_InitGPIO();

	/* Init scheduler */
	osKernelInitialize();

	tid_send = osThreadNew(Task_Send, NULL, &TaskSend_attributes);
	// tid_send_rnn = osThreadNew(Task_Send_RNN, NULL, &TaskSendRNN_attributes);

	/* USER CODE BEGIN RTOS_EVENTS */
	/* add events, ... */
	/* USER CODE END RTOS_EVENTS */

	/* Start scheduler */
	osKernelStart();

	/* We should never get here as control is now taken by the scheduler */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
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
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
		Error_Handler();
	}

	/** Configure LSE Drive Capability
	 */
	HAL_PWR_EnableBkUpAccess();
	__HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = 0;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 40;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
								  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		Error_Handler();
	}

	/** Enable MSI Auto calibration
	 */
	HAL_RCCEx_EnableMSIPLLMode();
}

/**
 * @brief I2C2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C2_Init(void) {
	/* USER CODE BEGIN I2C2_Init 0 */

	/* USER CODE END I2C2_Init 0 */

	/* USER CODE BEGIN I2C2_Init 1 */

	/* USER CODE END I2C2_Init 1 */
	hi2c2.Instance = I2C2;
	hi2c2.Init.Timing = 0x10D19CE4;
	hi2c2.Init.OwnAddress1 = 0;
	hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c2.Init.OwnAddress2 = 0;
	hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
		Error_Handler();
	}

	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
		Error_Handler();
	}

	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C2_Init 2 */

	/* USER CODE END I2C2_Init 2 */
}

/**
 * @brief QUADSPI Initialization Function
 * @param None
 * @retval None
 */
static void MX_QUADSPI_Init(void) {
	/* USER CODE BEGIN QUADSPI_Init 0 */

	/* USER CODE END QUADSPI_Init 0 */

	/* USER CODE BEGIN QUADSPI_Init 1 */

	/* USER CODE END QUADSPI_Init 1 */
	/* QUADSPI parameter configuration*/
	hqspi.Instance = QUADSPI;
	hqspi.Init.ClockPrescaler = 2;
	hqspi.Init.FifoThreshold = 4;
	hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
	hqspi.Init.FlashSize = 23;
	hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
	hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
	if (HAL_QSPI_Init(&hqspi) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN QUADSPI_Init 2 */

	/* USER CODE END QUADSPI_Init 2 */
}

/**
 * @brief SPI3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI3_Init(void) {
	/* USER CODE BEGIN SPI3_Init 0 */

	/* USER CODE END SPI3_Init 0 */

	/* USER CODE BEGIN SPI3_Init 1 */

	/* USER CODE END SPI3_Init 1 */
	/* SPI3 parameter configuration*/
	hspi3.Instance = SPI3;
	hspi3.Init.Mode = SPI_MODE_MASTER;
	hspi3.Init.Direction = SPI_DIRECTION_2LINES;
	hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi3.Init.NSS = SPI_NSS_SOFT;
	hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi3.Init.CRCPolynomial = 7;
	hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
	if (HAL_SPI_Init(&hspi3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI3_Init 2 */

	/* USER CODE END SPI3_Init 2 */
}

/**
 * @brief TIM16 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM16_Init(void) {
	/* USER CODE BEGIN TIM16_Init 0 */

	/* USER CODE END TIM16_Init 0 */

	/* USER CODE BEGIN TIM16_Init 1 */

	/* USER CODE END TIM16_Init 1 */
	htim16.Instance = TIM16;
	htim16.Init.Prescaler = 0;
	htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim16.Init.Period = 65535;
	htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim16.Init.RepetitionCounter = 0;
	htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim16) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM16_Init 2 */

	/* USER CODE END TIM16_Init 2 */
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
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
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
	huart3.Init.BaudRate = 115200;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART3_Init 2 */

	/* USER CODE END USART3_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(
		GPIOE, M24SR64_Y_RF_DISABLE_Pin | M24SR64_Y_GPO_Pin | ISM43362_RST_Pin,
		GPIO_PIN_RESET
	);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(
		GPIOA, ARD_D10_Pin | SPBTLE_RF_RST_Pin | ARD_D9_Pin, GPIO_PIN_RESET
	);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(
		GPIOB,
		ARD_D8_Pin | ISM43362_BOOT0_Pin | ISM43362_WAKEUP_Pin | LED2_Pin |
			SPSGRF_915_SDN_Pin | ARD_D5_Pin,
		GPIO_PIN_RESET
	);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(
		GPIOD, USB_OTG_FS_PWR_EN_Pin | PMOD_RESET_Pin | STSAFE_A100_RESET_Pin,
		GPIO_PIN_RESET
	);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(SPBTLE_RF_SPI3_CSN_GPIO_Port, SPBTLE_RF_SPI3_CSN_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, VL53L0X_XSHUT_Pin | LED3_WIFI__LED4_BLE_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(
		SPSGRF_915_SPI3_CSN_GPIO_Port, SPSGRF_915_SPI3_CSN_Pin, GPIO_PIN_SET
	);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(ISM43362_SPI3_CSN_GPIO_Port, ISM43362_SPI3_CSN_Pin, GPIO_PIN_SET);

	/*Configure GPIO pins : M24SR64_Y_RF_DISABLE_Pin M24SR64_Y_GPO_Pin ISM43362_RST_Pin
	 * ISM43362_SPI3_CSN_Pin */
	GPIO_InitStruct.Pin = M24SR64_Y_RF_DISABLE_Pin | M24SR64_Y_GPO_Pin |
						  ISM43362_RST_Pin | ISM43362_SPI3_CSN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pins : USB_OTG_FS_OVRCR_EXTI3_Pin SPSGRF_915_GPIO3_EXTI5_Pin
	 * SPBTLE_RF_IRQ_EXTI6_Pin ISM43362_DRDY_EXTI1_Pin */
	GPIO_InitStruct.Pin = USB_OTG_FS_OVRCR_EXTI3_Pin | SPSGRF_915_GPIO3_EXTI5_Pin |
						  SPBTLE_RF_IRQ_EXTI6_Pin | ISM43362_DRDY_EXTI1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pin : BUTTON_EXTI13_Pin */
	GPIO_InitStruct.Pin = BUTTON_EXTI13_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BUTTON_EXTI13_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : ARD_A5_Pin ARD_A4_Pin ARD_A3_Pin ARD_A2_Pin
							 ARD_A1_Pin ARD_A0_Pin */
	GPIO_InitStruct.Pin =
		ARD_A5_Pin | ARD_A4_Pin | ARD_A3_Pin | ARD_A2_Pin | ARD_A1_Pin | ARD_A0_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : ARD_D1_Pin ARD_D0_Pin */
	GPIO_InitStruct.Pin = ARD_D1_Pin | ARD_D0_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : ARD_D10_Pin SPBTLE_RF_RST_Pin ARD_D9_Pin */
	GPIO_InitStruct.Pin = ARD_D10_Pin | SPBTLE_RF_RST_Pin | ARD_D9_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : ARD_D4_Pin */
	GPIO_InitStruct.Pin = ARD_D4_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
	HAL_GPIO_Init(ARD_D4_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ARD_D7_Pin */
	GPIO_InitStruct.Pin = ARD_D7_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ARD_D7_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : ARD_D13_Pin ARD_D12_Pin ARD_D11_Pin */
	GPIO_InitStruct.Pin = ARD_D13_Pin | ARD_D12_Pin | ARD_D11_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : ARD_D3_Pin */
	GPIO_InitStruct.Pin = ARD_D3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ARD_D3_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ARD_D6_Pin */
	GPIO_InitStruct.Pin = ARD_D6_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ARD_D6_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : ARD_D8_Pin ISM43362_BOOT0_Pin ISM43362_WAKEUP_Pin LED2_Pin
							 SPSGRF_915_SDN_Pin ARD_D5_Pin SPSGRF_915_SPI3_CSN_Pin */
	GPIO_InitStruct.Pin = ARD_D8_Pin | ISM43362_BOOT0_Pin | ISM43362_WAKEUP_Pin |
						  LED2_Pin | SPSGRF_915_SDN_Pin | ARD_D5_Pin |
						  SPSGRF_915_SPI3_CSN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : DFSDM1_DATIN2_Pin DFSDM1_CKOUT_Pin */
	GPIO_InitStruct.Pin = DFSDM1_DATIN2_Pin | DFSDM1_CKOUT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF6_DFSDM1;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pins : LPS22HB_INT_DRDY_EXTI0_Pin LSM6DSL_INT1_EXTI11_Pin ARD_D2_Pin
	   HTS221_DRDY_EXTI15_Pin PMOD_IRQ_EXTI12_Pin */
	GPIO_InitStruct.Pin = LPS22HB_INT_DRDY_EXTI0_Pin | LSM6DSL_INT1_EXTI11_Pin |
						  ARD_D2_Pin | HTS221_DRDY_EXTI15_Pin | PMOD_IRQ_EXTI12_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pins : USB_OTG_FS_PWR_EN_Pin SPBTLE_RF_SPI3_CSN_Pin PMOD_RESET_Pin
	 * STSAFE_A100_RESET_Pin */
	GPIO_InitStruct.Pin = USB_OTG_FS_PWR_EN_Pin | SPBTLE_RF_SPI3_CSN_Pin |
						  PMOD_RESET_Pin | STSAFE_A100_RESET_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pins : VL53L0X_XSHUT_Pin LED3_WIFI__LED4_BLE_Pin */
	GPIO_InitStruct.Pin = VL53L0X_XSHUT_Pin | LED3_WIFI__LED4_BLE_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : VL53L0X_GPIO1_EXTI7_Pin LSM3MDL_DRDY_EXTI8_Pin */
	GPIO_InitStruct.Pin = VL53L0X_GPIO1_EXTI7_Pin | LSM3MDL_DRDY_EXTI8_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : PMOD_SPI2_SCK_Pin */
	GPIO_InitStruct.Pin = PMOD_SPI2_SCK_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(PMOD_SPI2_SCK_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : PMOD_UART2_CTS_Pin PMOD_UART2_RTS_Pin PMOD_UART2_TX_Pin
	 * PMOD_UART2_RX_Pin */
	GPIO_InitStruct.Pin =
		PMOD_UART2_CTS_Pin | PMOD_UART2_RTS_Pin | PMOD_UART2_TX_Pin | PMOD_UART2_RX_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pins : ARD_D15_Pin ARD_D14_Pin */
	GPIO_InitStruct.Pin = ARD_D15_Pin | ARD_D14_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void ACC_InitGPIO(void) {
	GPIO_InitTypeDef gpio_init_structure;
	gpio_init_structure.Pin = GPIO_PIN_11;
	gpio_init_structure.Mode = GPIO_MODE_IT_RISING;
	gpio_init_structure.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOD, &gpio_init_structure);
	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void Calibration_Init() {
	arm_mat_init_f32(&DtD, 6, 6, DtD_data);
	arm_mat_init_f32(&Dt1, 6, 1, Dt1_data);
	arm_mat_init_f32(&DtD_inv, 6, 6, DtD_inv_data);
	arm_mat_init_f32(&v, 6, 1, v_data);
	memset(DtD_data, 0, sizeof(DtD_data));
	memset(Dt1_data, 0, sizeof(Dt1_data));
}

void Add_Sample(int i) {
	float x = sample_data[i][0];
	float y = sample_data[i][1];
	float z = sample_data[i][2];
	float d[6];
	d[0] = x * x;
	d[1] = y * y;
	d[2] = z * z;
	d[3] = 2 * x;
	d[4] = 2 * y;
	d[5] = 2 * z;

	int k = 0;
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			DtD_data[k] += d[i] * d[j];
			++k;
		}
		Dt1_data[i] += d[i];
	}
}

int Calibration_Solve() {
	arm_status st;
	st = arm_mat_inverse_f32(&DtD, &DtD_inv);
	if (st != ARM_MATH_SUCCESS)
		return -1;
	st = arm_mat_mult_f32(&DtD_inv, &Dt1, &v);
	if (st != ARM_MATH_SUCCESS)
		return -1;

	float a = v_data[0];
	float b = v_data[1];
	float c = v_data[2];
	float g = v_data[3];
	float h = v_data[4];
	float i = v_data[5];

	cal_data.offset[0] = g / a;
	cal_data.offset[1] = h / b;
	cal_data.offset[2] = i / c;

	float G = 1 + (g * g) / a + (h * h) / b + (i * i) / c;

	arm_sqrt_f32(a / G, &cal_data.gain[0]);
	arm_sqrt_f32(b / G, &cal_data.gain[1]);
	arm_sqrt_f32(c / G, &cal_data.gain[2]);

	// to m / s^2
	cal_data.gain[0] *= 9.81;
	cal_data.gain[1] *= 9.81;
	cal_data.gain[2] *= 9.81;

	return 0;
}

void Calibration() {
	Calibration_Init();
	for (int i = 0; i < NUM_SAMPLE; ++i) {
		Add_Sample(i);
	}
	int ret = Calibration_Solve();
	if (ret != 0) {
		printf("Calibration error\n");
	}
}

float var(float *x, int n) {
	float sum = 0.0, sum2 = 0.0;
	for (int i = 0; i < n; ++i) {
		sum2 += x[i] * x[i];
		sum += x[i];
	}
	sum /= n;
	sum2 /= n;
	return sum2 - sum * sum;
}

void Task_Send(void *argument) {
	MX_USB_DEVICE_Init();
	Calibration();

	/*
	printf("\n");
	printf("%f, %f, %f\n", cal_data.offset[0], cal_data.offset[1], cal_data.offset[2]);
	printf("%f, %f, %f\n\n", cal_data.gain[0], cal_data.gain[1], cal_data.gain[2]);
	*/

	report = (report_t){
		.buttonMask = 0,
		.dx = 0,
		.dy = 0,
		.padding = 0,
	};

	int window = 12;
	float threshold = 0.21 * 0.21;
	int frames = 3;
	float alpha = 0.07;
	float dt = 1.0 / freq;

	float ax = 0.0, ay = 0.0;
	float vx = 0.0, vy = 0.0;
	float px = 0.0, py = 0.0;
	float bx = 0.0, by = 0.0;
	float buff_x[window];
	float buff_y[window];
	int ptr_x = 0, ptr_y = 0;
	int cx = 0, cy = 0;
	int count = 0;

	for (;;) {
		// calculate current time interval
		uint32_t deadline = osKernelGetTickCount() + osKernelGetTickFreq() / freq;

		// sample accelerometer data
		BSP_ACCELERO_AccGetXYZ(pDataXYZ);

		// calibration
		float data[2] = {pDataXYZ[0], pDataXYZ[1]};
		for (int i = 0; i < 2; ++i) {
			data[i] += cal_data.offset[i];
			data[i] *= cal_data.gain[i];
		}
		ax = -data[1], ay = data[0];

		buff_x[ptr_x++] = ax, buff_y[ptr_y++] = ay;
		++count;

		// x
		if (count >= window) {
			if (var(buff_x, window) < threshold) cx += 1;
			else cx = 0;
		}
		else cx += 1;
		if (cx >= frames) {
			vx = 0.0;
			bx = (1 - alpha) * bx + alpha * ax;
		} else {
			vx += (ax - bx) * dt;
			px += vx * dt;
		}
		
		// y
		if (count >= window) {
			if (var(buff_y, window) < threshold) cy += 1;
			else cy = 0;
		}
		else cy += 1;
		if (cy >= frames) {
			vy = 0.0;
			by = (1 - alpha) * by + alpha * ay;
		} else {
			vy += (ay - by) * dt;
			px += vx * dt;
		}

		// print velocity (cm), position (cm)
		printf("%f\t %f\t %f\t %f\n", vx * 100, vy * 100, px * 100, py * 100);
		
		report.dx = (char) (vx * 100);
		report.dy = (char) (vy * 100);
		
		USBD_HID_SendReport(&hUsbDeviceFS, (unsigned char *)&report, 4);
		osDelayUntil(deadline);
	}
}

void Task_Send_RNN(void *argument) {
	MX_USB_DEVICE_Init();
	// Calibration();

	const float A = 1.0011583795355363, B = 0.995068796668601, C = 1.0;
	const float D = -1.6759465583366173, E = -1.537611331891522, F = 8.423808603551663;

	report = (report_t){
		.buttonMask = 0,
		.dx = 0,
		.dy = 0,
		.padding = 0,
	};

	const float interval = 0.01;
	float hidden[LAYER_NUM][HIDDEN_SIZE] = {{0.0}},
		  hidden_next[LAYER_NUM][HIDDEN_SIZE] = {{0.0}};
	float output[2] = {0.0};
	float scale = 16;
	float buf_dx = 0, buf_dy = 0;
	for (;;) {
		// calculate current time interval
		uint32_t deadline = osKernelGetTickCount() + osKernelGetTickFreq() / freq;

		// sample accelerometer data
		BSP_ACCELERO_AccGetXYZ(pDataXYZ);
		// printf("%d, %d\n", pDataXYZ[0], pDataXYZ[1]);
		// calibration
		float data[2] = {pDataXYZ[0], pDataXYZ[1]};

		float ax = (data[1] - E) / B;
		float ay = -(data[0] - D) / A;

		rnn(ax, ay, hidden, output, hidden_next);
		for (int l = 0; l < LAYER_NUM; l++) {
			for (int i = 0; i < HIDDEN_SIZE; i++) {
				hidden[l][i] = hidden_next[l][i];
			}
		}

		float vx = output[0];
		float vy = output[1];

		buf_dx += vx * interval * rnn_alpha * scale;
		buf_dy -= vy * interval * rnn_alpha * scale;

		if (buf_dx * 0.5 > 127.0) {
			report.dx = 127;
		} else if (buf_dy * 0.5 <= -128.0) {
			report.dx = -128;
		} else {
			report.dx = (int)(buf_dx * 0.5);
		}
		if (buf_dy * 0.5 > 127.0) {
			report.dy = 127;
		} else if (buf_dy * 0.5 <= -128) {
			report.dy = -128;
		} else {
			report.dy = (int)(buf_dy * 0.5);
		}

		buf_dx -= report.dx;
		buf_dy -= report.dy;
		if (report.dx <= 3 && report.dx >= -3) {
			report.dx = 0;
		}
		if (report.dy <= 3 && report.dy >= -3) {
			report.dy = 0;
		}

		printf("%d, %d\n", report.dx, report.dy);

		USBD_HID_SendReport(&hUsbDeviceFS, (unsigned char *)&report, 4);
		osDelayUntil(deadline);
	}
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
