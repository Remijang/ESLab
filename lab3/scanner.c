/*
 *
 *  GattLib - GATT Library
 *
 *  Copyright (C) 2021-2024  Olivier Martin <olivier@labapart.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

// // #ifdef GATTLIB_LOG_BACKEND_SYSLOG
// #include <syslog.h>
// // #endif

#include "gattlib.h"

#define BLE_SCAN_TIMEOUT 5

#define GATTLIB_LOG_LEVEL 0

static const char* adapter_name;
static const char complete_name[] = "Lab3OWO";
static const char service_UUID[] = "11110000-1111-0000-1111-000011110000";
static const char CCCD_UUID[] = "00002902-0000-1000-8000-00805f9b34fb";
static const char notification_UUID_str[] = "ae191031-505b-473f-b95e-07da03d4fe3f";

typedef void (*ble_discovered_device_t)(const char* addr, const char* name);

// We use a mutex to make the BLE connections synchronous
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t m_connection_terminated = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t m_connection_terminated_lock = PTHREAD_MUTEX_INITIALIZER;

LIST_HEAD(listhead, connection_t) g_ble_connections;
struct connection_t {
	pthread_t thread;
	gattlib_adapter_t* adapter;
	char* addr;
	LIST_ENTRY(connection_t) entries;
};

static void notification_handler(const uuid_t* uuid,
								 const uint8_t* data,
								 size_t data_length,
								 void* user_data) {
	uintptr_t i;

	printf("Notification Handler: ");

	for (i = 0; i < data_length; i++) {
		printf("%02x ", data[i]);
	}
	printf("\n");
}

static void on_device_connect(gattlib_adapter_t* adapter,
							  const char* dst,
							  gattlib_connection_t* connection,
							  int error,
							  void* user_data) {
	gattlib_primary_service_t* services;
	gattlib_characteristic_t* characteristics;
	int services_count, characteristics_count;
	char uuid_str[MAX_LEN_UUID_STR + 1];
	int ret, i;

	ret = gattlib_discover_primary(connection, &services, &services_count);

	printf("%x %x\n", services, services_count);

	if (ret != GATTLIB_SUCCESS) {
		fprintf(stderr, "Fail to discover primary services.");
		goto EXIT;
	}

	gattlib_primary_service_t target_service;
	bool flag = false;
	for (i = 0; i < services_count; i++) {
		gattlib_uuid_to_string(&services[i].uuid, uuid_str, sizeof(uuid_str));
		if (strncmp(uuid_str, service_UUID, strlen(service_UUID)) == 0) {
			target_service = services[i];
			flag = true;
			printf("service[%d] start_handle:%02x end_handle:%02x uuid:%s\n",
				   i,
				   services[i].attr_handle_start,
				   services[i].attr_handle_end,
				   uuid_str);
			break;
		}
	}

	free(services);
	if (!flag) {
		printf("target service not found\n");
		goto EXIT;
	}
	ret = gattlib_discover_char_range(connection,
									  target_service.attr_handle_start,
									  target_service.attr_handle_end,
									  &characteristics,
									  &characteristics_count);
	if (ret != 0) {
		fprintf(stderr, "Fail to discover characteristics.");
		goto EXIT;
	}

	flag = false;
	gattlib_characteristic_t target_characteristic;
	for (i = 0; i < characteristics_count; i++) {
		gattlib_uuid_to_string(&characteristics[i].uuid, uuid_str, sizeof(uuid_str));
		if (strncmp(uuid_str, notification_UUID_str, strlen(notification_UUID_str)) ==
			0) {
			target_characteristic = characteristics[i];
			flag = true;
			printf("characteristic[%d] handle:%02x uuid:%s\n",
				   i,
				   characteristics[i].handle,
				   uuid_str);
			break;
		}
	}

	if (!flag) {
		printf("target characteristic not found\n");
		goto EXIT;
	}
	uint16_t enable_notification = 0x10;
	gattlib_write_char_by_handle(connection,
								 target_characteristic.handle + 1,
								 &enable_notification,
								 sizeof(enable_notification));
	ret = gattlib_register_notification(connection, notification_handler, NULL);
	if (ret != GATTLIB_SUCCESS) {
		fprintf(stderr, "Fail to register notification callback.");
		goto EXIT;
	}

	uuid_t notification_UUID;
	ret = gattlib_string_to_uuid(
		notification_UUID_str, strlen(notification_UUID_str) + 1, &notification_UUID);
	if (ret != GATTLIB_SUCCESS) {
		fprintf(stderr, "Wrong UUID format");
		goto EXIT;
	}

	ret = gattlib_notification_start(connection, &notification_UUID);
	if (ret != GATTLIB_SUCCESS) {
		fprintf(stderr, "Fail to start notification. Error: %x", ret);
		goto EXIT;
	}

	printf("Wait for notification for 20 seconds...");
	g_usleep(20 * G_USEC_PER_SEC);
	// free(characteristics);

EXIT:
	gattlib_disconnect(connection, true /* wait_disconnection */);
	pthread_mutex_lock(&m_connection_terminated_lock);
	pthread_cond_signal(&m_connection_terminated);
	pthread_mutex_unlock(&m_connection_terminated_lock);
}

static void* ble_connect_device(void* arg) {
	struct connection_t* connection = arg;
	char* addr = connection->addr;
	int ret;

	pthread_mutex_lock(&g_mutex);
	printf("------------START %s ---------------\n", addr);

	ret = gattlib_connect(connection->adapter,
						  connection->addr,
						  GATTLIB_CONNECTION_OPTIONS_LEGACY_DEFAULT,
						  on_device_connect,
						  NULL);
	if (ret != GATTLIB_SUCCESS) {
		fprintf(
			stderr, "Failed to connect to the bluetooth device '%s'", connection->addr);
	} else {
		printf("Successfully connect\n");
	}

	printf("------------DONE %s ---------------\n", addr);
	pthread_mutex_unlock(&g_mutex);
	return NULL;
}

static void ble_discovered_device(gattlib_adapter_t* adapter,
								  const char* addr,
								  const char* name,
								  void* user_data) {
	struct connection_t* connection;
	int ret;

	if (name == NULL || strncmp(name, complete_name, strlen(complete_name)) != 0) {
		return;
	}

	printf("Discovered %s - '%s'\n", addr, name);
	connection = calloc(sizeof(struct connection_t), 1);
	if (connection == NULL) {
		fprintf(stderr, "Failt to allocate connection.");
		return;
	}
	connection->addr = strdup(addr);
	connection->adapter = adapter;

	ret = pthread_create(&connection->thread, NULL, ble_connect_device, connection);
	if (ret != 0) {
		fprintf(stderr, "Failt to create BLE connection thread.");
		free(connection);
		return;
	}
	LIST_INSERT_HEAD(&g_ble_connections, connection, entries);
}

static void* ble_task(void* arg) {
	gattlib_adapter_t* adapter;
	int ret;

	ret = gattlib_adapter_open(adapter_name, &adapter);
	if (ret) {
		fprintf(stderr, "Failed to open adapter.");
		return NULL;
	}

	pthread_mutex_lock(&g_mutex);
	ret = gattlib_adapter_scan_enable(
		adapter, ble_discovered_device, BLE_SCAN_TIMEOUT, NULL /* user_data */);
	if (ret) {
		fprintf(stderr, "Failed to scan.");
		goto EXIT;
	}

	puts("Scan completed");
	pthread_mutex_unlock(&g_mutex);

	// Wait for the thread to complete
	while (g_ble_connections.lh_first != NULL) {
		struct connection_t* connection = g_ble_connections.lh_first;
		pthread_join(connection->thread, NULL);
		LIST_REMOVE(g_ble_connections.lh_first, entries);
		free(connection->addr);
		free(connection);
	}

EXIT:
	pthread_mutex_lock(&m_connection_terminated_lock);
	pthread_cond_wait(&m_connection_terminated, &m_connection_terminated_lock);
	pthread_mutex_unlock(&m_connection_terminated_lock);
	gattlib_adapter_scan_disable(adapter);
	gattlib_adapter_close(adapter);
	return NULL;
}

int main(int argc, const char* argv[]) {
	int ret;

	if (argc == 1) {
		adapter_name = NULL;
	} else if (argc == 2) {
		adapter_name = argv[1];
	} else {
		printf("%s [<bluetooth-adapter>]\n", argv[0]);
		return 1;
	}

	// openlog("gattlib_ble_scan", LOG_CONS | LOG_NDELAY | LOG_PERROR, LOG_USER);
	// setlogmask(LOG_UPTO(LOG_INFO));

	LIST_INIT(&g_ble_connections);

	ret = gattlib_mainloop(ble_task, NULL);
	if (ret != GATTLIB_SUCCESS) {
		fprintf(stderr, "Failed to create gattlib mainloop");
	}

	return ret;
}
