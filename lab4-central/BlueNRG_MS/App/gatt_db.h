/**
 ******************************************************************************
 * @file    App/gatt_db.h
 * @author  SRA Application Team
 * @brief   Header file for App/gatt_db.c
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

#ifndef GATT_DB_H
#define GATT_DB_H

#include <stdint.h>
#include <stdlib.h>

#include "bluenrg_aci_const.h"
#include "bluenrg_def.h"
#include "bluenrg_gap_aci.h"
#include "cmsis_os2.h"
#include "sensor.h"

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

/**
 * @brief Structure containing acceleration value of each axis.
 */
typedef struct {
	int32_t AXIS_X;
	int32_t AXIS_Y;
	int32_t AXIS_Z;
} AxesRaw_t;

enum { ACCELERATION_SERVICE_INDEX = 0, ENVIRONMENTAL_SERVICE_INDEX = 1 };

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

tBleStatus Add_HWServW2ST_Service(void);
tBleStatus Add_SWServW2ST_Service(void);
void Read_Request_CB(uint16_t handle);
tBleStatus Acc_Update(AxesRaw_t *x_axes);
void Write_Request_CB(uint16_t handle, uint8_t *data, uint8_t length);

void GAP_Device_Found_CB(uint8_t *data);
void GAP_Procedure_Complete_CB(evt_gap_procedure_complete *data);
uint8_t parse_advertising_data(
	uint8_t *data, uint8_t data_length, char *buf, uint8_t buf_len
);

extern uint8_t Services_Max_Attribute_Records[];

#endif /* GATT_DB_H */
