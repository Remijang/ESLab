/**
 ******************************************************************************
 * @file    app_bluenrg_ms.h
 * @author  SRA Application Team
 * @brief   Header file for app_bluenrg_ms.c
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef APP_BLUENRG_MS_H
#define APP_BLUENRG_MS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdlib.h>

#include "bluenrg_aci_const.h"
#include "bluenrg_def.h"
#include "bluenrg_gap_aci.h"
#include "cmsis_os2.h"
#include "sensor.h"

#define COPY_ACC_GYRO_MAG_W2ST_CHAR_UUID(uuid_struct)                                  \
	COPY_UUID_128(                                                                     \
		uuid_struct, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x01, 0x11, 0xe1, 0xac, 0x36, 0x00, \
		0x02, 0xa5, 0xd5, 0xc5, 0x1b                                                   \
	)

#define COPY_SAMPLE_FREQUENCY_CHAR_UUID(uuid_struct)                                   \
	COPY_UUID_128(                                                                     \
		uuid_struct, 0x11, 0x11, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x11, 0x11, 0x00, \
		0x00, 0x11, 0x11, 0x00, 0x00                                                   \
	)

#define COPY_CCCD_UUID(uuid_struct)                                                    \
	COPY_UUID_128(                                                                     \
		uuid_struct, 0x00, 0x00, 0x29, 0x02, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, \
		0x80, 0x5f, 0x9b, 0x34, 0xfb                                                   \
	)

/* Exported Functions --------------------------------------------------------*/
void MX_BlueNRG_MS_Init(void);
void MX_Process_Event(void);
void MX_Start_Scanning(void);
void MX_Stop_Scanning(void);
void MX_Connect_Peripheral(void);
void MX_Discover_Characteristic(uint8_t uuid_type, const uint8_t *uuid);
void MX_Enable_Notification(uint16_t char_handle);
void MX_Write_Data(uint16_t char_handle, uint8_t *data, uint8_t length);

#define X_OFFSET 200
#define Y_OFFSET 50
#define Z_OFFSET 1000

/**
 * @brief Number of application services
 */
#define NUMBER_OF_APPLICATION_SERVICES (2)

/**
 * @brief Define How Many quaterions you want to transmit (from 1 to 3)
 *        In this sample application use only 1
 */
#define SEND_N_QUATERNIONS 1

enum { ACCELERATION_SERVICE_INDEX = 0, ENVIRONMENTAL_SERVICE_INDEX = 1 };

extern uint8_t Services_Max_Attribute_Records[];
#include "cmsis_os2.h"
extern osMutexId_t print_mutex;
#define PRINTF(...) printf(__VA_ARGS__)
#define THREAD_PRINTF(...)                                    \
	if (osMutexAcquire(print_mutex, osWaitForever) == osOK) { \
		PRINTF(__VA_ARGS__);                                  \
		osMutexRelease(print_mutex);                          \
	}
#ifdef __cplusplus
}
#endif
#endif /* APP_BLUENRG_MS_H */
