/**
 ******************************************************************************
 * @file    App/sensor.c
 * @author  SRA Application Team
 * @brief   Sensor init and sensor state machines
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
#include "sensor.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluenrg_aci_const.h"
#include "bluenrg_gap.h"
#include "bluenrg_gap_aci.h"
#include "bluenrg_gatt_aci.h"
#include "cmsis_os2.h"
#include "hci_const.h"
#include "hci_le.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define ADV_INTERVAL_MIN_MS 1000
#define ADV_INTERVAL_MAX_MS 1200

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern uint8_t bdaddr[BDADDR_SIZE];
extern uint8_t bnrg_expansion_board;
__IO uint8_t set_connectable = 1;
__IO uint16_t connection_handle = 0;
__IO uint8_t notification_enabled = FALSE;
__IO uint32_t connected = FALSE;

extern uint16_t EnvironmentalCharHandle;
extern uint16_t AccGyroMagCharHandle;
extern uint16_t DiscoveredHandle;
extern uint8_t msg;
extern const char complete_name[];
extern uint8_t dev_bdaddr[BDADDR_SIZE];

volatile uint8_t request_free_fall_notify = FALSE;

AxesRaw_t x_axes = {0, 0, 0};
AxesRaw_t g_axes = {0, 0, 0};
AxesRaw_t m_axes = {0, 0, 0};

extern uint8_t target_type;
extern UUID_t target_uuid;

/* Private function prototypes -----------------------------------------------*/
void GAP_DisconnectionComplete_CB(void);
void GAP_ConnectionComplete_CB(uint8_t addr[6], uint16_t handle);
void GAP_Device_Found_CB(uint8_t *data);
void GAP_Procedure_Complete_CB(evt_gap_procedure_complete *data);
uint8_t parse_advertising_data(
	uint8_t *data, uint8_t data_length, char *buf, uint8_t buf_len
);
void GATT_Procedure_Complete_CB(evt_gatt_procedure_complete *data);
void GATT_Discover_Read_Char_By_UUID_CB(evt_gatt_disc_read_char_by_uuid_resp *data);
void ATT_Find_Info_CB(evt_att_find_information_resp *data);
bool Is_Identical_UUID(UUID_t uuid1, UUID_t uuid2, uint8_t type, bool reverse);

/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
 * Function Name  : Set_DeviceConnectable.
 * Description    : Puts the device in connectable mode.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void Set_DeviceConnectable(void) {
	uint8_t ret;
	const char local_name[] = {AD_TYPE_COMPLETE_LOCAL_NAME, SENSOR_DEMO_NAME};

	uint8_t manuf_data[26] = {
		2,
		0x0A,
		0x00,
		/* 0 dBm */	 // Transmission Power
		8,
		0x09,
		SENSOR_DEMO_NAME,  // Complete Name
		13,
		0xFF,
		0x01, /* SKD version */
		0x80,
		0x00,
		0xF4,	   /* ACC+Gyro+Mag 0xE0 | 0x04 Temp | 0x10 Pressure */
		0x00,	   /*  */
		0x00,	   /*  */
		bdaddr[5], /* BLE MAC start -MSB first- */
		bdaddr[4],
		bdaddr[3],
		bdaddr[2],
		bdaddr[1],
		bdaddr[0] /* BLE MAC stop */
	};

	manuf_data[18] |= 0x01; /* Sensor Fusion */

	hci_le_set_scan_resp_data(0, NULL);

	PRINTF("Set General Discoverable Mode.\n");

	ret = aci_gap_set_discoverable(
		ADV_DATA_TYPE, (ADV_INTERVAL_MIN_MS * 1000) / 625,
		(ADV_INTERVAL_MAX_MS * 1000) / 625, STATIC_RANDOM_ADDR, NO_WHITE_LIST_USE,
		sizeof(local_name), local_name, 0, NULL, 0, 0
	);

	aci_gap_update_adv_data(26, manuf_data);

	if (ret != BLE_STATUS_SUCCESS) {
		PRINTF("aci_gap_set_discoverable() failed: 0x%02X\r\n", ret);
	} else
		PRINTF("aci_gap_set_discoverable() --> SUCCESS\r\n");
}

void dump_packet(hci_event_pckt *event_pckt) {
	for (uint8_t i = 0; i < event_pckt->plen; i++) {
		PRINTF("%02X", event_pckt->data[i]);
		if ((i + 1) % 16 == 0)
			PRINTF("\n");
	}
	PRINTF("\n");
}
/**
 * @brief  Callback processing the ACI events.
 * @note   Inside this function each event must be identified and correctly
 *         parsed.
 * @param  void* Pointer to the ACI packet
 * @retval None
 */
void user_notify(void *pData) {
	hci_uart_pckt *hci_pckt = pData;
	/* obtain event packet */
	hci_event_pckt *event_pckt = (hci_event_pckt *)hci_pckt->data;
	if (hci_pckt->type != HCI_EVENT_PKT)
		return;

	switch (event_pckt->evt) {
		case EVT_DISCONN_COMPLETE: {
			GAP_DisconnectionComplete_CB();
		} break;
		case EVT_LE_META_EVENT: {
			evt_le_meta_event *evt = (void *)event_pckt->data;

			switch (evt->subevent) {
				case EVT_LE_CONN_COMPLETE: {
					evt_le_connection_complete *cc = (void *)evt->data;
					GAP_ConnectionComplete_CB(cc->peer_bdaddr, cc->handle);
				} break;
				case EVT_LE_ADVERTISING_REPORT: {
					GAP_Device_Found_CB(evt->data + 1);
				} break;
			}
		} break;
		case EVT_VENDOR: {
			evt_blue_aci *blue_evt = (void *)event_pckt->data;
			switch (blue_evt->ecode) {
				case EVT_BLUE_GATT_PROCEDURE_COMPLETE: {
					evt_gatt_procedure_complete *data = (void *)blue_evt->data;
					GATT_Procedure_Complete_CB(data);
				} break;
				case EVT_BLUE_GATT_DISC_READ_CHAR_BY_UUID_RESP: {
					evt_gatt_disc_read_char_by_uuid_resp *data = (void *)blue_evt->data;
					GATT_Discover_Read_Char_By_UUID_CB(data);
				} break;
				case EVT_BLUE_ATT_FIND_INFORMATION_RESP: {
					evt_att_find_information_resp *data = (void *)blue_evt->data;
					ATT_Find_Info_CB(data);
				} break;
				case EVT_BLUE_GAP_PROCEDURE_COMPLETE: {
					evt_gap_procedure_complete *data =
						(evt_gap_procedure_complete *)blue_evt->data;
					GAP_Procedure_Complete_CB(data);
				} break;
				case EVT_BLUE_GATT_NOTIFICATION: {
					evt_gatt_attr_notification *data =
						(evt_gatt_attr_notification *)blue_evt->data;
					Notification_Handler(data);
				}
			}
		} break;
	}
}

/**
 * @brief  This function is called when the peer device gets disconnected.
 * @param  None
 * @retval None
 */
void GAP_DisconnectionComplete_CB(void) {
	connected = FALSE;
	PRINTF("Disconnected\n");
	/* Make the device connectable again. */
	set_connectable = TRUE;
	notification_enabled = FALSE;
}

/**
 * @brief  This function is called when there is a LE Connection Complete event.
 * @param  uint8_t Address of peer device
 * @param  uint16_t Connection handle
 * @retval None
 */
void GAP_ConnectionComplete_CB(uint8_t addr[6], uint16_t handle) {
	connected = TRUE;
	connection_handle = handle;

	PRINTF("Connected to device:");
	for (uint32_t i = 5; i > 0; i--) {
		PRINTF("%02X-", addr[i]);
	}
	PRINTF("%02X\n", addr[0]);
	PRINTF("\n---  Connect Complete  ---\n");
	msg++;
}

void GAP_Device_Found_CB(uint8_t *data) {
	le_advertising_info *le_info = (le_advertising_info *)data;
	static char buf[16];

	uint8_t ret =
		parse_advertising_data(le_info->data_RSSI, le_info->data_length, buf, 16);
	if (ret == 1 && strncmp(buf, complete_name, strlen(complete_name)) == 0) {
		for (int i = 0; i < 6; i++)
			dev_bdaddr[i] = le_info->bdaddr[i];
		PRINTF(
			"Found device %02X:%02X:%02X:%02X:%02X:%02X, complete name: %s\n",
			dev_bdaddr[0], dev_bdaddr[1], dev_bdaddr[2], dev_bdaddr[3], dev_bdaddr[4],
			dev_bdaddr[5], buf
		);
		msg++;
	}
}

void GAP_Procedure_Complete_CB(evt_gap_procedure_complete *data) {
	switch (data->procedure_code) {
		case GAP_GENERAL_DISCOVERY_PROC:
			PRINTF("---  End of Scan  ---\n\n");
			msg++;
			break;
		default:
			PRINTF("---  Procedure Complete: %d  ---\n\n", data->procedure_code);
			return;
	}
}

void GATT_Procedure_Complete_CB(evt_gatt_procedure_complete *data) {
	PRINTF("---  GATT Procedure Complete  ---\n");
	msg++;
}

uint8_t parse_advertising_data(
	uint8_t *data, uint8_t data_length, char *buf, uint8_t buf_len
) {
	if (data == 0) {
		PRINTF("No complete name found.\n");
		return 0;
	}
	uint8_t index = 0;
	while (index < data_length - 1) {  // -1 to avoid reading RSSI
		uint8_t field_length = data[index];
		if (field_length == 0)
			break;

		uint8_t ad_type = data[index + 1];

		if (ad_type == 0x09 || ad_type == 0x08) {  // Complete Local Name
			uint8_t name_len = field_length - 1;
			if (name_len > buf_len - 1)
				name_len = buf_len - 1;
			memcpy(buf, &data[index + 2], name_len);
			buf[name_len] = '\0';
			PRINTF("Device name: %s\n", buf);
			return 1;
		}

		index += field_length + 1;	// Move to next AD structure
	}
	PRINTF("No complete name found.\n");
	return 0;
}

void GATT_Discover_Read_Char_By_UUID_CB(evt_gatt_disc_read_char_by_uuid_resp *data) {
	DiscoveredHandle = data->attr_handle;
	PRINTF("    Found Handle 0x%04X\n", DiscoveredHandle);
}

void ATT_Find_Info_CB(evt_att_find_information_resp *data) {
	PRINTF("    Info format: %d\n", data->format);
	PRINTF(
		"    Info handle: %02X%02X\n    Info UUID: ", data->handle_uuid_pair[0],
		data->handle_uuid_pair[1]
	);
	for (int i = 0; i < 16; i++) {
		PRINTF("%02X", data->handle_uuid_pair[2 + i]);
	}
	PRINTF("\n");

	UUID_t uuid;
	if (target_type == UUID_TYPE_128) {
		for (int i = 0; i < 16; i++) {
			uuid.UUID_128[i] = data->handle_uuid_pair[2 + 15 - i];
		}
		if (data->format != 1 &&
			Is_Identical_UUID(target_uuid, uuid, UUID_TYPE_128, false)) {
			DiscoveredHandle = *(uint16_t *)data->handle_uuid_pair;
		}
		return;
	}
	if (data->format != 2 &&
		Is_Identical_UUID(
			target_uuid, (UUID_t)(*(uint16_t *)(data->handle_uuid_pair + 2)),
			UUID_TYPE_16, false
		)) {
		DiscoveredHandle = *(uint16_t *)data->handle_uuid_pair;
	}
	return;
}

bool Is_Identical_UUID(UUID_t uuid1, UUID_t uuid2, uint8_t type, bool reverse) {
	switch (type) {
		case UUID_TYPE_128:
			for (int i = 0; i < 16; i++) {
				if (!reverse && uuid1.UUID_128[i] != uuid2.UUID_128[i])
					return false;
				if (reverse && uuid1.UUID_128[i] != uuid2.UUID_128[15 - i])
					return false;
			}
			return true;
		case UUID_TYPE_16:
			if (reverse)
				uuid2.UUID_16 = (uuid2.UUID_16 >> 8) + (uuid2.UUID_16 << 8);
			return uuid1.UUID_16 == uuid2.UUID_16;
		default:
			return false;
	}
}

void Notification_Handler(evt_gatt_attr_notification *data) {
	PRINTF("    Get Notification: ");
	for (int i = 0; i < data->event_data_length - 2; i++) {
		PRINTF("%02X", data->attr_value[i]);
	}
	PRINTF("\n");
}