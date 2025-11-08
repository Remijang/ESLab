/**
 ******************************************************************************
 * @file    App/sensor.h
 * @author  SRA Application Team
 * @brief   Header file for App/sensor.c
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

#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>

#define IDB04A1 0
#define IDB05A1 1
#define SENSOR_DEMO_NAME 'C', 'e', 'n', 't', 'r', 'a', 'l'
#define BDADDR_SIZE 6

void Set_DeviceConnectable(void);
void user_notify(void *pData);

/** @brief Macro that stores Value into a buffer in Little Endian Format (2 bytes)*/
#define HOST_TO_LE_16(buf, val) \
	(((buf)[0] = (uint8_t)(val)), ((buf)[1] = (uint8_t)(val >> 8)))

#define COPY_UUID_128(                                                                 \
	uuid_struct, uuid_15, uuid_14, uuid_13, uuid_12, uuid_11, uuid_10, uuid_9, uuid_8, \
	uuid_7, uuid_6, uuid_5, uuid_4, uuid_3, uuid_2, uuid_1, uuid_0                     \
)                                                                                      \
	do {                                                                               \
		uuid_struct[0] = uuid_0;                                                       \
		uuid_struct[1] = uuid_1;                                                       \
		uuid_struct[2] = uuid_2;                                                       \
		uuid_struct[3] = uuid_3;                                                       \
		uuid_struct[4] = uuid_4;                                                       \
		uuid_struct[5] = uuid_5;                                                       \
		uuid_struct[6] = uuid_6;                                                       \
		uuid_struct[7] = uuid_7;                                                       \
		uuid_struct[8] = uuid_8;                                                       \
		uuid_struct[9] = uuid_9;                                                       \
		uuid_struct[10] = uuid_10;                                                     \
		uuid_struct[11] = uuid_11;                                                     \
		uuid_struct[12] = uuid_12;                                                     \
		uuid_struct[13] = uuid_13;                                                     \
		uuid_struct[14] = uuid_14;                                                     \
		uuid_struct[15] = uuid_15;                                                     \
	} while (0)

/**
 * @brief Structure containing acceleration value of each axis.
 */
typedef struct {
	int32_t AXIS_X;
	int32_t AXIS_Y;
	int32_t AXIS_Z;
} AxesRaw_t;

/** Documentation for C union Service_UUID_t */
typedef union Service_UUID_t_s {
	/** 16-bit UUID
	 */
	uint16_t Service_UUID_16;
	/** 128-bit UUID
	 */
	uint8_t Service_UUID_128[16];
} Service_UUID_t;

/** Documentation for C union Char_UUID_t */
typedef union Char_UUID_t_s {
	/** 16-bit UUID
	 */
	uint16_t Char_UUID_16;
	/** 128-bit UUID
	 */
	uint8_t Char_UUID_128[16];
} Char_UUID_t;

typedef union UUID_t_s {
	/** 16-bit UUID
	 */
	uint16_t UUID_16;
	/** 128-bit UUID
	 */
	uint8_t UUID_128[16];
} UUID_t;

extern uint8_t Application_Max_Attribute_Records[];
#include "cmsis_os2.h"
extern osMutexId_t print_mutex;
#define PRINTF(...) printf(__VA_ARGS__)
#define THREAD_PRINTF(...)                                    \
	if (osMutexAcquire(print_mutex, osWaitForever) == osOK) { \
		PRINTF(__VA_ARGS__);                                  \
		osMutexRelease(print_mutex);                          \
	}
#endif /* SENSOR_H */
