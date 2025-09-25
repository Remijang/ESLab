#include "http.h"

#include <string.h>

#include "wifi.h"

HTTP_Status_t HTTP_Method(uint8_t RemoteIP[],
						  uint16_t RemotePORT,
						  const uint8_t *pdata,
						  uint16_t Reqlen,
						  uint16_t *SentDatalen,
						  uint32_t Timeout,
						  HTTP_Method_t method,
						  char *path) {
	uint32_t socket = -1;

	if (WIFI_OpenClientConnection(
			0, WIFI_TCP_PROTOCOL, "TCP_CLIENT", RemoteIP, RemotePORT, 0) ==
		WIFI_STATUS_OK) {
		socket = 0;
	}

	if (socket == -1) {
		return HTTP_CONNECTION_FAIL;
	}

	char packet[256] = "";

	switch (method) {
		case HTTP_GET:
			sprintf(packet, "GET %s HTTP/1.0\r\n", pdata);
			break;
		case HTTP_POST:
			sprintf(
				packet,
				"POST %s HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: "
				"%d\r\n\r\n%s",
				path,
				Reqlen,
				pdata);
			break;
	}
	uint16_t DataLen;
	WIFI_Status_t status =
		WIFI_SendData(socket, (uint8_t *)packet, strlen(packet), &DataLen, Timeout);
	if (status == WIFI_STATUS_ERROR)
		return HTTP_SERVER_ERROR;
	if (DataLen != strlen(packet))
		return HTTP_CONTINUE;
	(void)WIFI_CloseClientConnection(socket);
	return HTTP_SUCCESS;
}