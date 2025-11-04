/**
 ******************************************************************************
 * @file    App/gatt_db.c
 * @author  SRA Application Team
 * @brief   Functions to build GATT DB and handle GATT events
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

#include "gatt_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluenrg_conf.h"
#include "bluenrg_def.h"
#include "bluenrg_gap.h"
#include "bluenrg_gap_aci.h"
#include "bluenrg_gatt_aci.h"
#include "llist.h"

/** @brief Macro that stores Value into a buffer in Little Endian Format (2 bytes)*/
#define HOST_TO_LE_16(buf, val) \
	(((buf)[0] = (uint8_t)(val)), ((buf)[1] = (uint8_t)(val >> 8)))

/** @brief Macro that stores Value into a buffer in Little Endian Format (4 bytes) */
#define HOST_TO_LE_32(buf, val)                                     \
	(((buf)[0] = (uint8_t)(val)), ((buf)[1] = (uint8_t)(val >> 8)), \
	 ((buf)[2] = (uint8_t)(val >> 16)), ((buf)[3] = (uint8_t)(val >> 24)))

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

/* Hardware Characteristics Service */
#define COPY_HW_SENS_W2ST_SERVICE_UUID(uuid_struct)                                    \
	COPY_UUID_128(                                                                     \
		uuid_struct, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0xe1, 0x9a, 0xb4, 0x00, \
		0x02, 0xa5, 0xd5, 0xc5, 0x1b                                                   \
	)
#define COPY_ENVIRONMENTAL_W2ST_CHAR_UUID(uuid_struct)                                 \
	COPY_UUID_128(                                                                     \
		uuid_struct, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0xe1, 0xac, 0x36, 0x00, \
		0x02, 0xa5, 0xd5, 0xc5, 0x1b                                                   \
	)
#define COPY_ACC_GYRO_MAG_W2ST_CHAR_UUID(uuid_struct)                                  \
	COPY_UUID_128(                                                                     \
		uuid_struct, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x01, 0x11, 0xe1, 0xac, 0x36, 0x00, \
		0x02, 0xa5, 0xd5, 0xc5, 0x1b                                                   \
	)
/* Software Characteristics Service */
#define COPY_SW_SENS_W2ST_SERVICE_UUID(uuid_struct)                                    \
	COPY_UUID_128(                                                                     \
		uuid_struct, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x11, 0xe1, 0x9a, 0xb4, 0x00, \
		0x02, 0xa5, 0xd5, 0xc5, 0x1b                                                   \
	)
#define COPY_QUATERNIONS_W2ST_CHAR_UUID(uuid_struct)                                   \
	COPY_UUID_128(                                                                     \
		uuid_struct, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x11, 0xe1, 0xac, 0x36, 0x00, \
		0x02, 0xa5, 0xd5, 0xc5, 0x1b                                                   \
	)

#define COPY_SAMPLE_FREQUENCY_CHAR_UUID(uuid_struct)                                   \
	COPY_UUID_128(                                                                     \
		uuid_struct, 0x11, 0x11, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x11, 0x11, 0x00, \
		0x00, 0x11, 0x11, 0x00, 0x00                                                   \
	)

uint16_t HWServW2STHandle, EnvironmentalCharHandle, AccGyroMagCharHandle, FrequencyHandle;
uint16_t SWServW2STHandle, QuaternionsCharHandle;

/* UUIDS */
Service_UUID_t service_uuid;
Char_UUID_t char_uuid;

extern AxesRaw_t x_axes;
extern AxesRaw_t g_axes;
extern AxesRaw_t m_axes;
extern uint32_t freq;

extern uint16_t connection_handle;
extern uint32_t start_time;
extern llist *adver_list;

/**
 * @brief  Add the 'HW' service (and the Environmental and AccGyr characteristics).
 * @param  None
 * @retval tBleStatus Status
 */
tBleStatus Add_HWServW2ST_Service(void) {
	tBleStatus ret;
	uint8_t uuid[16];

	/* Add_HWServW2ST_Service */
	COPY_HW_SENS_W2ST_SERVICE_UUID(uuid);
	BLUENRG_memcpy(&service_uuid.Service_UUID_128, uuid, 16);
	ret = aci_gatt_add_serv(
		UUID_TYPE_128, service_uuid.Service_UUID_128, PRIMARY_SERVICE, 1 + 4 * 5,
		&HWServW2STHandle
	);
	if (ret != BLE_STATUS_SUCCESS)
		return BLE_STATUS_ERROR;

	/* Fill the Environmental BLE Characteristc */
	COPY_ENVIRONMENTAL_W2ST_CHAR_UUID(uuid);
	uuid[14] |= 0x04; /* One Temperature value*/
	uuid[14] |= 0x10; /* Pressure value*/
	BLUENRG_memcpy(&char_uuid.Char_UUID_128, uuid, 16);
	ret = aci_gatt_add_char(
		HWServW2STHandle, UUID_TYPE_128, char_uuid.Char_UUID_128, 2 + 2 + 4,
		CHAR_PROP_NOTIFY | CHAR_PROP_READ, ATTR_PERMISSION_NONE,
		GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP, 16, 0, &EnvironmentalCharHandle
	);
	if (ret != BLE_STATUS_SUCCESS)
		return BLE_STATUS_ERROR;

	/* Fill the AccGyroMag BLE Characteristc */
	COPY_ACC_GYRO_MAG_W2ST_CHAR_UUID(uuid);
	BLUENRG_memcpy(&char_uuid.Char_UUID_128, uuid, 16);
	ret = aci_gatt_add_char(
		HWServW2STHandle, UUID_TYPE_128, char_uuid.Char_UUID_128, 2 + 3 * 2,
		CHAR_PROP_NOTIFY | CHAR_PROP_READ, ATTR_PERMISSION_NONE,
		GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP, 16, 0, &AccGyroMagCharHandle
	);
	if (ret != BLE_STATUS_SUCCESS)
		return BLE_STATUS_ERROR;

	COPY_SAMPLE_FREQUENCY_CHAR_UUID(uuid);
	BLUENRG_memcpy(&char_uuid.Char_UUID_128, uuid, 16);
	ret = aci_gatt_add_char(
		HWServW2STHandle, UUID_TYPE_128, char_uuid.Char_UUID_128, 4,
		CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP, ATTR_PERMISSION_NONE,
		GATT_NOTIFY_ATTRIBUTE_WRITE, 16, 0, &FrequencyHandle
	);
	if (ret != BLE_STATUS_SUCCESS)
		return BLE_STATUS_ERROR;
	return BLE_STATUS_SUCCESS;
}

/**
 * @brief  Add the SW Feature service using a vendor specific profile
 * @param  None
 * @retval tBleStatus Status
 */
tBleStatus Add_SWServW2ST_Service(void) {
	tBleStatus ret;
	int32_t NumberOfRecords = 1;
	uint8_t uuid[16];

	COPY_SW_SENS_W2ST_SERVICE_UUID(uuid);
	BLUENRG_memcpy(&service_uuid.Service_UUID_128, uuid, 16);
	ret = aci_gatt_add_serv(
		UUID_TYPE_128, service_uuid.Service_UUID_128, PRIMARY_SERVICE,
		1 + 3 * NumberOfRecords, &SWServW2STHandle
	);

	if (ret != BLE_STATUS_SUCCESS) {
		goto fail;
	}

	COPY_QUATERNIONS_W2ST_CHAR_UUID(uuid);
	BLUENRG_memcpy(&char_uuid.Char_UUID_128, uuid, 16);
	ret = aci_gatt_add_char(
		SWServW2STHandle, UUID_TYPE_128, char_uuid.Char_UUID_128,
		2 + 6 * SEND_N_QUATERNIONS, CHAR_PROP_NOTIFY, ATTR_PERMISSION_NONE,
		GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP, 16, 0, &QuaternionsCharHandle
	);

	if (ret != BLE_STATUS_SUCCESS) {
		goto fail;
	}

	return BLE_STATUS_SUCCESS;

fail:
	return BLE_STATUS_ERROR;
}

/**
 * @brief  Update acceleration characteristic value
 * @param  AxesRaw_t structure containing acceleration value in mg.
 * @retval tBleStatus Status
 */
tBleStatus Acc_Update(AxesRaw_t *x_axes) {
	uint8_t buff[2 + 2 * 3];
	tBleStatus ret;

	HOST_TO_LE_16(buff, (HAL_GetTick() >> 3));

	HOST_TO_LE_16(buff + 2, (uint16_t)x_axes->AXIS_X);
	HOST_TO_LE_16(buff + 4, (uint16_t)x_axes->AXIS_Y);
	HOST_TO_LE_16(buff + 6, (uint16_t)x_axes->AXIS_Z);

	ret = aci_gatt_update_char_value_ext_IDB05A1(
		HWServW2STHandle, AccGyroMagCharHandle, NOTIFICATION, 8, 0, 2 + 2 * 3, buff
	);
	if (ret != BLE_STATUS_SUCCESS) {
		PRINTF("Error while updating Acceleration characteristic: 0x%02X\n", ret);
		return BLE_STATUS_ERROR;
	}

	return BLE_STATUS_SUCCESS;
}

/*******************************************************************************
 * Function Name  : Read_Request_CB.
 * Description    : Update the sensor values.
 * Input          : Handle of the characteristic to update.
 * Return         : None.
 *******************************************************************************/
void Read_Request_CB(uint16_t handle) {
	PRINTF("Read Requested\n");
	tBleStatus ret;

	if (handle == AccGyroMagCharHandle + 1) {
		Acc_Update(&x_axes);
	}

	if (connection_handle != 0) {
		ret = aci_gatt_allow_read(connection_handle);
		if (ret != BLE_STATUS_SUCCESS) {
			PRINTF("aci_gatt_allow_read() failed: 0x%02x\r\n", ret);
		}
	}
}

void Frequency_Update(uint32_t *freq, uint8_t *data, uint8_t length) {
	if (!freq || !data)
		return;
	*freq = *(uint32_t *)(data);
}

void Write_Request_CB(uint16_t handle, uint8_t *data, uint8_t length) {
	if (handle == FrequencyHandle + 1) {
		Frequency_Update(&freq, data, length);
	}
}

void GAP_Device_Found_CB(void *data, uint16_t flag) {
	tBDAddr addr;
	llist_push(adver_list, data);
	if (flag == EVT_LE_ADVERTISING_REPORT) {
		le_advertising_info *le_info = (le_advertising_info *)data;
		for (int i = 0; i < 6; i++)
			addr[i] = le_info->bdaddr[i];
	} else {
		evt_gap_device_found *evt_device = (evt_gap_device_found *)data;
		for (int i = 0; i < 6; i++)
			addr[i] = evt_device->bdaddr[i];
	}
	PRINTF(
		"\rFound device %02x:%02x:%02x:%02x:%02x:%02x\n", addr[0], addr[1], addr[2],
		addr[3], addr[4], addr[5]
	);
}

void GAP_Procedure_Complete_CB(evt_gap_procedure_complete *data) {
	switch (data->procedure_code) {
		case GAP_GENERAL_DISCOVERY_PROC:
			PRINTF("---  End of Scan  ---\n\n");

		case GAP_NAME_DISCOVERY_PROC:
			PRINTF("---  Name Discovered ---\n\n");
			break;
		default:
			return;
	}
	tBDAddr addr;
	void *data = NULL;
	if ((data = llist_pop(adver_list)) == NULL)
		return;
	le_advertising_info *le_info = (le_advertising_info *)data;
	for (int i = 0; i < 6; i++)
		addr[i] = le_info->bdaddr[i];
	tBleStatus ret = aci_gap_start_name_discovery_proc(
		0x0050, 0x0030, RANDOM_ADDR, addr, STATIC_RANDOM_ADDR, 0x0010, 0x0020, 0x0000,
		0x0100, 0x0000, 0xffff
	);
	if (ret == BLE_STATUS_TIMEOUT)
		PRINTF("Request Timeout\n");
	else if (ret != BLE_STATUS_SUCCESS)
		PRINTF("Fail to name discovery, error code %04x\n\n", ret);
	break;
}

void parse_advertising_data(uint8_t *data, uint8_t data_length) {
	uint8_t index = 0;
	// for (int i = 0; i < data_length; i++) {
	// 	PRINTF("%02x ", data[i]);
	// }
	while (index < data_length - 1) {  // -1 to avoid reading RSSI
		uint8_t field_length = data[index];
		if (field_length == 0)
			break;

		uint8_t ad_type = data[index + 1];

		if (ad_type == 0x09 || ad_type == 0x08) {  // Complete Local Name
			uint8_t name_len = field_length - 1;
			if (name_len > sizeof(complete_name) - 1)
				name_len = sizeof(complete_name) - 1;
			memcpy(complete_name, &data[index + 2], name_len);
			complete_name[name_len] = '\0';
			PRINTF("Device name: %s\n", complete_name);
			return;
		}

		index += field_length + 1;	// Move to next AD structure
	}
	PRINTF("No complete name found.\n");
}
