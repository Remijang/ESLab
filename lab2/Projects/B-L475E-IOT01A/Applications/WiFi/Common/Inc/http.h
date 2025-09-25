#ifndef HTTP_H
#define HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wifi.h"

typedef enum { HTTP_GET, HTTP_POST } HTTP_Method_t;
typedef enum {
	HTTP_SERVER_ERROR,
	HTTP_CONTINUE,
	HTTP_SUCCESS,
	HTTP_CONNECTION_FAIL
} HTTP_Status_t;

HTTP_Status_t HTTP_Method(uint8_t RemoteIP[],
						  uint16_t RemotePORT,
						  const uint8_t *pdata,
						  uint16_t Reqlen,
						  uint16_t *SentDatalen,
						  uint32_t Timeout,
						  HTTP_Method_t method,
						  char *path);

#ifdef __cplusplus
}
#endif
#endif