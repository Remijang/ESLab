#include "acc_helper.h"

void ACC_SingnificantMotionDetectionOn(void) {
	uint8_t tmp;

	/* Read CTRL1_XL */
	tmp = SENSOR_IO_Read(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_INT1_CTRL);
	tmp |= 0x40;
	SENSOR_IO_Write(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_INT1_CTRL, tmp);

	tmp = SENSOR_IO_Read(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_CTRL10_C);
	tmp |= 0x05;
	SENSOR_IO_Write(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_CTRL10_C, tmp);

	tmp = SENSOR_IO_Read(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_TAP_CFG1);
	tmp |= 0x10;
	SENSOR_IO_Write(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_TAP_CFG1, tmp);
}

void ACC_SingnificantMotionDetectionOff(void) {
	uint8_t tmp;

	/* Read CTRL1_XL */
	tmp = SENSOR_IO_Read(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_INT1_CTRL);
	tmp &= ~(0x40);
	SENSOR_IO_Write(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_INT1_CTRL, tmp);
}

void ACC_InitGPIO(void) {
	GPIO_InitTypeDef gpio_init_structure;
	gpio_init_structure.Pin = GPIO_PIN_11;
	gpio_init_structure.Mode = GPIO_MODE_IT_RISING;
	gpio_init_structure.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOD, &gpio_init_structure);
	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}
