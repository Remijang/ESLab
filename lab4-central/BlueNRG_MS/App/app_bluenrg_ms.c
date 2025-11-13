/**
 ******************************************************************************
 * @file    app_bluenrg_ms.c
 * @author  SRA Application Team
 * @brief   BlueNRG-M0 initialization and applicative code
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

/* Includes ------------------------------------------------------------------*/
#include "app_bluenrg_ms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "b_l475e_iot01a1.h"
#include "bluenrg_conf.h"
#include "bluenrg_def.h"
#include "bluenrg_gap.h"
#include "bluenrg_gap_aci.h"
#include "bluenrg_gatt_aci.h"
#include "bluenrg_hal_aci.h"
#include "bluenrg_utils.h"
#include "cmsis_os2.h"
#include "compiler.h"
#include "hci.h"
#include "hci_const.h"
#include "hci_le.h"
#include "hci_tl.h"
#include "link_layer.h"
#include "sensor.h"
#include "sm.h"
#include "stm32l475e_iot01_accelero.h"
#include "stm32l4xx_hal_tim.h"

/* Private function prototypes -----------------------------------------------*/

static void User_Init(void);

/* Private defines -----------------------------------------------------------*/
/**
 * 1 to send environmental and motion data when pushing the user button
 * 0 to send environmental and motion data automatically (period = 1 sec)
 */
#define USE_BUTTON 0

/* Private macros ------------------------------------------------------------*/
/** @brief Macro that stores Value into a buffer in Little Endian Format (2 bytes)*/
#define HOST_TO_LE_16(buf, val) \
	(((buf)[0] = (uint8_t)(val)), ((buf)[1] = (uint8_t)(val >> 8)))

/** @brief Macro that stores Value into a buffer in Little Endian Format (4 bytes) */
#define HOST_TO_LE_32(buf, val)                                     \
	(((buf)[0] = (uint8_t)(val)), ((buf)[1] = (uint8_t)(val >> 8)), \
	 ((buf)[2] = (uint8_t)(val >> 16)), ((buf)[3] = (uint8_t)(val >> 24)))

/* Private variables ---------------------------------------------------------*/
extern uint32_t freq;

uint8_t target_type;
UUID_t target_uuid;
extern uint16_t DiscoveredHandle;
extern AxesRaw_t x_axes;

extern uint16_t connection_handle;
extern uint32_t start_time;

uint8_t dev_bdaddr[BDADDR_SIZE];
const char complete_name[] = "Lab4OWO";

extern volatile uint8_t set_connectable;
extern volatile int connected;
extern uint16_t connection_handle;

/* at startup, suppose the X-NUCLEO-IDB04A1 is used */
uint8_t bnrg_expansion_board = IDB04A1;
uint8_t bdaddr[BDADDR_SIZE];
static volatile uint8_t user_button_init_state = 1;
static volatile uint8_t user_button_pressed = 0;

extern uint8_t msg;

/* Private function prototypes -----------------------------------------------*/
#if PRINT_CSV_FORMAT
extern volatile uint32_t ms_counter;
/**
 * @brief  This function is a utility to print the log time
 *         in the format HH:MM:SS:MSS (DK GUI time format)
 * @param  None
 * @retval None
 */
void print_csv_time(void) {
	uint32_t ms = HAL_GetTick();
	PRINT_CSV(
		"%02ld:%02ld:%02ld.%03ld", (long)(ms / (60 * 60 * 1000) % 24),
		(long)(ms / (60 * 1000) % 60), (long)((ms / 1000) % 60), (long)(ms % 1000)
	);
}
#endif

// /**
//  * @brief  Update acceleration characteristic value
//  * @param  AxesRaw_t structure containing acceleration value in mg.
//  * @retval tBleStatus Status
//  */
// tBleStatus Acc_Update(AxesRaw_t *x_axes) {
// 	uint8_t buff[2 + 2 * 3];
// 	tBleStatus ret;

// 	HOST_TO_LE_16(buff, (HAL_GetTick() >> 3));

// 	HOST_TO_LE_16(buff + 2, (uint16_t)x_axes->AXIS_X);
// 	HOST_TO_LE_16(buff + 4, (uint16_t)x_axes->AXIS_Y);
// 	HOST_TO_LE_16(buff + 6, (uint16_t)x_axes->AXIS_Z);

// 	ret = aci_gatt_update_char_value_ext_IDB05A1(
// 		HWServW2STHandle, AccGyroMagCharHandle, NOTIFICATION, 8, 0, 2 + 2 * 3, buff
// 	);
// 	if (ret != BLE_STATUS_SUCCESS) {
// 		THREAD_PRINTF("Error while updating Acceleration characteristic: 0x%02X\n", ret);
// 		return BLE_STATUS_ERROR;
// 	}

// 	return BLE_STATUS_SUCCESS;
// }

void MX_BlueNRG_MS_Init(void) {
	/* Initialize the peripherals and the BLE Stack */
	const char *name = "Central";
	uint16_t service_handle, dev_name_char_handle, appearance_char_handle;
	uint8_t bdaddr_len_out;
	uint8_t hwVersion;
	uint16_t fwVersion;
	int ret;
	User_Init();
	/* Get the User Button initial state */
	user_button_init_state = BSP_PB_GetState(BUTTON_KEY);
	hci_init(user_notify, NULL);
	/* get the BlueNRG HW and FW versions */
	getBlueNRGVersion(&hwVersion, &fwVersion);
	/*
	 * Reset BlueNRG again otherwise we won't
	 * be able to change its MAC address.
	 * aci_hal_write_config_data() must be the first
	 * command after reset otherwise it will fail.
	 */
	hci_reset();
	HAL_Delay(100);
	THREAD_PRINTF("HWver %d\nFWver %d\n", hwVersion, fwVersion);
	if (hwVersion > 0x30) { /* X-NUCLEO-IDB05A1 expansion board is used */
		bnrg_expansion_board = IDB05A1;
	}
	ret = aci_hal_read_config_data(
		CONFIG_DATA_RANDOM_ADDRESS, BDADDR_SIZE, &bdaddr_len_out, bdaddr
	);
	if (ret) {
		THREAD_PRINTF("Read Static Random address failed.\n");
	}
	if ((bdaddr[5] & 0xC0) != 0xC0) {
		THREAD_PRINTF("Static Random address not well formed.\n");
		while (1) {}
	}
	THREAD_PRINTF(
		"Device address: %02X:%02X:%02X:%02X:%02X:%02X\n", bdaddr[0], bdaddr[1],
		bdaddr[2], bdaddr[3], bdaddr[4], bdaddr[5]
	);
	/* GATT Init */
	ret = aci_gatt_init();
	if (ret) {
		THREAD_PRINTF("GATT_Init failed.\n");
	}

	/* GAP Init */
	if (bnrg_expansion_board == IDB05A1) {
		ret = aci_gap_init_IDB05A1(
			GAP_CENTRAL_ROLE_IDB05A1, 0, 0x07, &service_handle, &dev_name_char_handle,
			&appearance_char_handle
		);
	} else {
		ret = aci_gap_init_IDB04A1(
			GAP_CENTRAL_ROLE_IDB04A1, &service_handle, &dev_name_char_handle,
			&appearance_char_handle
		);
	}
	if (ret != BLE_STATUS_SUCCESS) {
		THREAD_PRINTF("GAP_Init failed.\n");
	}
	/* Update device name */
	ret = aci_gatt_update_char_value(
		service_handle, dev_name_char_handle, 0, strlen(name), (uint8_t *)name
	);
	if (ret) {
		THREAD_PRINTF("aci_gatt_update_char_value failed.\n");
		while (1) {}
	}
	THREAD_PRINTF("BLE Stack Initialized\n");
	/* Set output power level */
	ret = aci_hal_set_tx_power_level(1, 4);
}

static void User_Init(void) {
	BSP_COM_Init(COM1);
}

void MX_Start_Scanning(void) {
	uint16_t scanInterval = 0x0010;
	tBleStatus ret = aci_gap_start_general_discovery_proc(
		scanInterval, scanInterval, STATIC_RANDOM_ADDR, 0x01
	);
	if (ret != BLE_STATUS_SUCCESS) {
		THREAD_PRINTF("Error occurs\n");
	} else {
		THREAD_PRINTF("--- Start of Scan ---\n");
	}
}

void MX_Stop_Scanning(void) {
	tBleStatus ret;
	do {
		ret = aci_gap_terminate_gap_procedure(GAP_GENERAL_DISCOVERY_PROC);
	} while (ret == BLE_STATUS_TIMEOUT);
	if (ret != BLE_STATUS_SUCCESS) {
		THREAD_PRINTF("Failed to stop scanning, error: 0x%02X\n", ret);
	} else {
		THREAD_PRINTF("--- Stop Scanning ---\n");
	}
}

void MX_Connect_Peripheral(void) {
	tBleStatus ret;
	uint16_t scanInterval = 0x0040;	 // 40 ms
	uint16_t scanWindow = 0x0040;	 // 40 ms

	uint8_t peer_bdaddr_type = RANDOM_ADDR;
	uint8_t own_bdaddr_type = STATIC_RANDOM_ADDR;

	uint16_t conn_min_interval = 0x0006;  // 7.5 ms
	uint16_t conn_max_interval = 0x0080;  // 15 ms
	uint16_t conn_latency = 0x0000;
	uint16_t supervision_timeout = 0x01F4;	// 500 (5s)
	uint16_t min_conn_length = 0x0000;
	uint16_t max_conn_length = 0xFFFF;

	THREAD_PRINTF("\n--  Start Connecting to peripheral  ---\n");

	ret = aci_gap_create_connection(
		scanInterval, scanWindow, peer_bdaddr_type, dev_bdaddr, own_bdaddr_type,
		conn_min_interval, conn_max_interval, conn_latency, supervision_timeout,
		min_conn_length, max_conn_length
	);

	if (ret == BLE_STATUS_SUCCESS) {
		THREAD_PRINTF("aci_gap_create_connection: OK\n");
	} else {
		THREAD_PRINTF("aci_gap_create_connection failed (0x%02X)\n", ret);
	}
}

void MX_Discover_Characteristic(uint8_t uuid_type, const uint8_t *uuid) {
	tBleStatus ret =
		aci_gatt_disc_charac_by_uuid(connection_handle, 0x0001, 0xffff, uuid_type, uuid);
	if (ret != BLE_STATUS_SUCCESS) {
		THREAD_PRINTF("Failed to start characteristic discovery by UUID: 0x%02X\n", ret);
	} else {
		THREAD_PRINTF("---  Start Discovering characteristic: ");
		for (int i = 0; i < 16; i++) {
			THREAD_PRINTF("%02X", uuid[i]);
		}
		THREAD_PRINTF("  ---\n");
	}
}

void MX_Enable_Notification(uint16_t char_handle) {
	target_type = UUID_TYPE_16;
	target_uuid.UUID_16 = 0x2902;
	DiscoveredHandle = 0x0000;
	tBleStatus ret =
		aci_gatt_disc_all_charac_descriptors(connection_handle, char_handle + 1, 0xFFFF);
	if (ret != BLE_STATUS_SUCCESS) {
		THREAD_PRINTF("Failed to find notification handle: 0x%02X\n", ret);
	} else {
		THREAD_PRINTF("---  Finding Notification Handle ---\n");
	}
	while (1) {
		if (msg == 1)
			break;
		hci_user_evt_proc();
		HAL_Delay(25);
	}
	msg--;
	if (DiscoveredHandle == 0x0000) {
		THREAD_PRINTF("    Notification Handle Not Found\n");
		exit(1);
		return;
	}
	THREAD_PRINTF("    Find notification handle: %04X\n", DiscoveredHandle);
	uint8_t data[2] = {0x01, 0x00};
	ret = aci_gatt_write_charac_descriptor(connection_handle, DiscoveredHandle, 2, data);
	if (ret != BLE_STATUS_SUCCESS) {
		THREAD_PRINTF("Failed to enable notification: 0x%02X\n", ret);
	} else {
		THREAD_PRINTF("--- Start Writing 0x0100 to Notification Handle ---\n");
	}
	while (1) {
		if (msg == 1)
			break;
		hci_user_evt_proc();
		HAL_Delay(25);
	}
	msg--;
}

/*
 * BlueNRG-MS background task
 */
void MX_Process_Event(void) {
	hci_user_evt_proc();
}

void MX_Write_Data(uint16_t char_handle, uint8_t *data, uint8_t length) {
	tBleStatus ret =
		aci_gatt_write_charac_value(connection_handle, char_handle, length, data);
	if (ret != BLE_STATUS_SUCCESS) {
		THREAD_PRINTF("Error writing data: %02X\n", ret);
		return;
	} else {
		THREAD_PRINTF("Start writing data\n");
	}
	while (1) {
		if (msg == 1)
			break;
		hci_user_evt_proc();
		HAL_Delay(25);
	}
	msg--;
	return;
}
