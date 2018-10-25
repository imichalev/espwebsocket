/*
 * meWebsosketClient.h
 *
 *  Created on: 4.08.2018 ã.
 *      Author: imihalev
 */

#ifndef MAIN_MEWEBSOCKETCLIENT_H_
#define MAIN_MEWEBSOCKETCLIENT_H_



#endif /* MAIN_MEWEBSOCKETCLIENT_H_ */


#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"


typedef struct esp_ws_client  *esp_ws_client_handle_t;



typedef enum{
	WS_EVENT_ERROR = 0,
	WS_EVENT_CONNECTED,
    WS_EVENT_DISCONNECTED,
	WS_EVENT_DATA,
}esp_ws_event_id_t;



typedef struct{
  esp_ws_event_id_t event_id;
  esp_ws_client_handle_t client;
  char *data;
  int data_len;
  uint8_t data_type;
}esp_ws_event_t;


typedef esp_ws_event_t* esp_ws_event_handle_t;
typedef esp_err_t  (*ws_event_callback_t)(esp_ws_event_handle_t event);

typedef struct {
  	int task_stack;
  	int task_ptio;
	char *serverIP;
	int  serverPort;
	int localPort;
	int network_timeout_ms;
	bool reconnect;
	ws_event_callback_t event_handle;
}ws_client_config_t;

typedef enum {
    WS_STATE_ERROR = -1,
    WS_STATE_UNKNOWN = 0,
    WS_STATE_INIT,
    WS_STATE_CONNECTED,
    WS_STATE_WAIT_TIMEOUT,
} ws_client_state_t;

struct esp_ws_client{
	ws_client_state_t state;
	ws_client_config_t *config;
	//bool run;
	bool run_ping;
	EventGroupHandle_t status_bits;
	//SemaphoreHandle_t  xSemaphore;
	struct netconn *wsconn;
	esp_ws_event_t event;
};

typedef struct {
	const char *serverIP;
	const char *uri;
	uint32_t serverPort;
	uint32_t localPort;
	int keepalive;
	bool reconnect;
	ws_event_callback_t event_handle;
	//bool clientOk; //state
} esp_ws_client_config_t;

typedef struct {
  union {
    struct {
      uint16_t LEN:7;     // bits 0..  6
      uint16_t MASK:1;    // bit  7
      uint16_t OPCODE:4;  // bits 8..  11
      uint16_t :3;        // bits 12.. 14 reserved
      uint16_t FIN:1;     // bit  15
    } bit;
    struct {
      uint16_t ONE:8;     // bits 0..  7
      uint16_t ZERO:8;    // bits 8..  15
    } pos;
  } param; // the initial parameters of the header
  uint64_t length; // actual message length
  union {
    char part[4]; // the mask, array
    uint32_t full; // the mask, all 32 bits
  } key; // masking key
  bool received; // was a message successfully received?
} ws_header_t;


#define WS_MASK_L		0x4		/**< \brief Length of MASK field in WebSocket Header*/
typedef struct {
	uint8_t opcode :WS_MASK_L;
	uint8_t reserved :3;
	uint8_t FIN :1;
	uint8_t payload_length :7;
	uint8_t mask :1;
} WS_frame_header_t;

/** \brief Opcode according to RFC 6455*/
typedef enum {
	WS_OP_CON = 0x0, 				/*!< Continuation Frame*/
	WS_OP_TXT = 0x1, 				/*!< Text Frame*/
	WS_OP_BIN = 0x2, 				/*!< Binary Frame*/
	WS_OP_CLS = 0x8, 				/*!< Connection Close Frame*/
	WS_OP_PIN = 0x9, 				/*!< Ping Frame*/
	WS_OP_PON = 0xa 				/*!< Pong Frame*/
} WS_OPCODES;





#define ESP_MEM_CHECK(TAG, a, action) if (!(a)) {                                                      \
        ESP_LOGE(TAG,"%s:%d (%s): %s", __FILE__, __LINE__, __FUNCTION__, "Memory exhausted");       \
        action;                                                                                         \
        }

#define WS_DEFAULT_LOCAL_PORT  4040
#define WS_DEFAULT_SERVER_PORT 8080



void ws_client_connect(void);
esp_ws_client_handle_t esp_ws_client_init(const esp_ws_client_config_t *config);
esp_err_t esp_ws_client_start(esp_ws_client_handle_t client );
esp_err_t ws_write(esp_ws_client_handle_t client,char *data,int len);
esp_err_t ws_client_ping(esp_ws_client_handle_t client);
esp_err_t esp_ws_client_stop(esp_ws_client_handle_t client);






