/*
 * meWebsosketClient.c
 *
 *  Created on: 4.08.2018 �.
 *      Author: imihalev
 */


#include "ws_client.h"
#include "freertos/FreeRTOS.h"
//#include "esp_heap_alloc_caps.h"
//#include "hwcrypto/sha.h"
//#include "esp_system.h"
#include <string.h>
#include <stdlib.h>
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/ip4_addr.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include <sys/time.h>
//#include "time.h"
#include "freertos/semphr.h"
#include "esp_heap_trace.h"
//#include "freertos/portable.h"

const static int STOPPED_BIT = (1<<0);
const static int KEEPALIVE_BIT=(1<<1);
const static int PONANSWER_BIT=(1<<2);
const static int SERVERDOWN_BIT=(1<<3);
const static int KEEPALIVEDOWN=(1<<4);
const static int ESPWSTASKRUN=(1<<5);

static  char TAG[20];
#define WS_STD_LEN			125		/**< \brief Maximum Length of standard length frames*/
#define RECVTIMEOUT 10000 //10s 
#define KEEAPALIVETIME 20000 //20s 
#define null ((void*)0);

 void systemtime(){
	time_t now;
	time(&now);
	struct tm timeinfo;
	localtime_r(&now, &timeinfo);
	sprintf(TAG,"WS_CLIENT %02d:%02d:%02d ",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
}


static esp_err_t esp_ws_send_event(esp_ws_client_handle_t client){
	err_t err=ESP_OK;
	client->event.client=client;
	if(client->config->event_handle){
   	 return client->config->event_handle(&client->event);
	}
	return ESP_FAIL;
}


static bool check_task(char* taskname) {

	TaskStatus_t *pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;
	uint32_t ulTotalRunTime;
	uxArraySize = uxTaskGetNumberOfTasks();
	bool ret = false;
	pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
	if (pxTaskStatusArray != NULL) {
		uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize,
				&ulTotalRunTime);
		for (x = 0; x < uxArraySize; x++) {
			if (!strcmp(taskname, pxTaskStatusArray[x].pcTaskName)) {
				ret = true;
				break;
			}
		}
	}
	vPortFree(pxTaskStatusArray);
	return ret;
}

//Read
err_t recv_task(struct netconn *conn, char* buffer,uint16_t buflen) {
	err_t err = ERR_OK;
	//systemtime();
	//ESP_LOGE(TAG, "recv_task");
	if(conn==NULL){
		systemtime();
		ESP_LOGE(TAG,"recv_task conn==NULL");
		return -1;
	}

//  struct pbuf *buf;
//	err= netconn_recv_tcp_pbuf(conn,&buf);
//	if(err==ESP_OK){
//	memcpy(buffer,buf->payload,buf->len);
//	}


	             //inbuf=netbuf_new();
	//uint16_t buflen=len;
	struct netbuf *inbuf; //Memory leak
                  //netconn_set_recvtimeout(conn,1000)				
	err=netconn_recv(conn, &inbuf);  //Here stop and Waiting for result from received....
	if (err == ERR_OK) {
	             //netbuf_data(inbuf,(void**)&buffer, &buflen); don't use this !!!!!!!!
	    netbuf_copy(inbuf,buffer,buflen);
		//if(inbuf)netbuf_delete(inbuf);
		
	}
	//printf("recv_task err:%d\n",err);
	if(inbuf)netbuf_delete(inbuf);
	return err;
}

static char *removewhitespace(char *str) {
	char *result;
	int len = strlen(str);
	result = malloc(len);
	bzero(result, len);
	int k = 0;
	for (int i = 0; i <= len; i++) {
		if (str[i] != ' ') {
			result[k++] = str[i];
		}
	}
	memcpy(str,result,strlen(result));
	str[strlen(result)]=0;
	free(result);
	return str;
}

static char *get_http_header(char *buffer, const char *refer)
{
	char *data = strstr(buffer, refer);
	if (data)
	{
		data += strlen(refer);
		char *the_end = strstr(data, "\r\n");
		if (the_end)
		{
			the_end[0] = 0; //Mark the end of data
			//Remove space from data
			return removewhitespace(data);
		}
	}
	return NULL;
}

static err_t esp_ws_set_config(esp_ws_client_handle_t client, const esp_ws_client_config_t *config)
{
	err_t err = ESP_OK;
	ws_client_config_t *cfg = calloc(1, sizeof(ws_client_config_t));
	client->config = cfg;
	if (config->localPort < 10000)
	{
		cfg->localPort = config->localPort;
	}
	else
	{
		cfg->localPort = (rand() & 0x7FFF) + 10000;
	}
	if (config->serverPort != 0)
	{
		cfg->serverPort = config->serverPort;
	}
	else
	{
		cfg->localPort = WS_DEFAULT_SERVER_PORT;
	}

	cfg->serverIP = strdup(config->serverIP);
	cfg->reconnect = config->reconnect;
	cfg->event_handle = config->event_handle;

	return err;
}

static void make_send_buffer(uint8_t *pSendBuffer, WS_frame_header_t hdr, uint32_t key, char *p_buf, uint8_t len)
{
	uint8_t *pSource = (uint8_t *)&hdr;
	for (int8_t i = 0; i < sizeof(WS_frame_header_t); i++)
	{
		*pSendBuffer++ = *pSource++;
	}
	pSource = (uint8_t *)&key;
	for (int8_t i = 0; i < sizeof(uint32_t); i++)
	{
		*pSendBuffer++ = *pSource++;
	}
	pSource = (uint8_t *)p_buf;
	for (int8_t i = 0; i < len; i++)
	{
		*pSendBuffer++ = *pSource++;
	}
}



static uint32_t random_key()
{
	uint32_t key = (rand() & 0xFFFFFFFF);
	//ESP_LOGI(TAG, "Random Key:0x%x",key);
	return key;
}

static err_t code_data(char *data, uint8_t len, uint32_t key)
{
	err_t err = ESP_OK;
	if (len == 0)
	{
		return err;
	}
	uint8_t *p;
	uint8_t *pEnd;
	p = (uint8_t *)&key;
	pEnd = p + 4;
	char temp;
	for (uint8_t i = 0; i < len; i++)
	{
		temp = data[i];
		temp = (uint8_t)temp ^ (*p++);
		if (p == pEnd)
		{
			p = (uint8_t *)&key;
		}
		data[i] = temp;
	}
	return err;
}

static err_t  ping_respond(esp_ws_client_handle_t client){
	err_t err=ESP_OK;
	char *buffer;
	buffer=(char*)calloc(10,sizeof(char));
	struct netconn *conn=client->wsconn;
	if(conn==NULL){
		ESP_LOGE(TAG, "ws_read conn is null.\n");
		return -1;
	}
	err=recv_task(conn,buffer,10);          //recv_task waiting for incoming.Programm stop hear!
	 if(err ==ERR_OK){
	   // systemtime();
		//printf("I have got  some data:,<%s>\n",buffer);
		WS_frame_header_t* p_frame_hdr;
		p_frame_hdr = (WS_frame_header_t*) buffer;
		 if(p_frame_hdr->opcode==WS_OP_PON){  //pon from server no data received.
			 systemtime();
		   	  printf("PON answer.\n");
		   	 xEventGroupSetBits(client->status_bits,PONANSWER_BIT);
		  }else{
          err=-1;
		  }
		}
    return err;
}


static err_t esp_ws_connect(esp_ws_client_handle_t client) {
	systemtime();
	uint16_t localPort=(rand()& 0x7FFF) + 10000;
				//	if(client->config->localPort == localPort){
				//		localPort=(rand()& 0x7FFF) + 10000;
				//	}
	client->config->localPort=localPort+(rand()&0xFF);
	ESP_LOGI(TAG, "esp_ws_connect-->going to connect by local port:%d",client->config->localPort);
	err_t err = ERR_OK;
			//Resolve DNS -- Latter
			//	struct hostent *he;
			//	he = gethostbyname(host);
	struct netconn *conn;
			//Try to connect to Web Socket Server.... and Check Authentication key.... and so so....
	static ip_addr_t ServerIpAddress;
	ip4addr_aton(client->config->serverIP, &(ServerIpAddress.u_addr.ip4));
	conn = netconn_new(NETCONN_TCP);
	if (conn != NULL) {
		err = netconn_bind(conn, NULL, client->config->localPort);
		if (err == ERR_OK) {
			conn->recv_timeout=RECVTIMEOUT;
			err = netconn_connect(conn,(const ip_addr_t*)&ServerIpAddress,client->config->serverPort);
			if (err == ERR_OK) {
				//Send Hello to Server and Who am I ?
				char *buffer;
				buffer = malloc(1024 * sizeof(char));
				//char buffer[1024];
				//Make client_key
				unsigned char random_key[16] = { 0 }, client_key[32] = { 0 };
				int i;
				for (i = 0; i < sizeof(random_key); i++) {
					random_key[i] = rand() & 0xFF;
				}
				size_t outlen = 0;
				mbedtls_base64_encode(client_key, 32, &outlen, random_key, 16);
				char *path = "/";
				char ipv4_str[16];
				sprintf(ipv4_str, "0.0.0.0");
				tcpip_adapter_ip_info_t if_info;
				if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA,
						&if_info)==ESP_OK) {
					ip4addr_ntoa_r(&(if_info.ip), ipv4_str, sizeof(ipv4_str));
				} else if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP,
						&if_info) == ESP_OK) {
					ip4addr_ntoa_r(&(if_info.ip), ipv4_str, sizeof(ipv4_str));
				}
				//printf("ipv4_str:%s\n", ipv4_str);
				int port = client->config->localPort;
				bzero(buffer, 1024);
				snprintf(buffer, 1024, "GET %s HTTP/1.1\r\n"
						"Host: %s:%d\r\n"
						"Connection: Upgrade\r\n"
						"Upgrade: websocket\r\n"
						"Sec-WebSocket-Version: 13\r\n"
						//"Sec-WebSocket-Protocol: mqtt\r\n"
						"Sec-WebSocket-Key: %s\r\n"
						"User-Agent: ESP32 MQTT Client\r\n\r\n", path, ipv4_str,
						port, client_key);
				if (netconn_write(conn, buffer,strlen(buffer),NETCONN_NOCOPY) == ERR_OK) {
					bzero(buffer, 1024);
					if (recv_task(conn, buffer,1024) == ERR_OK) {
						//First Hello from Server  find sec_key
						//printf("WebSocket data received:<%s>\n",buffer);
						char *server_key = get_http_header(buffer,
								"Sec-WebSocket-Accept:");
						systemtime();
						//ESP_LOGE(TAG,"sever sec_key:<%s>\n", server_key);
						if (server_key == NULL) {
							systemtime();
							ESP_LOGE(TAG,
									"Error no Server key Found.\nI am going to Exit.\n");
							err=ESP_FAIL;
							goto END;
						}
						//if ok Check server request...
						unsigned char client_key_b64[64], valid_client_key[20],
								accept_key[32] = { 0 };
						int key_len = sprintf((char*) client_key_b64,
								"%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
								(char*) client_key);
						//printf("client_key_b64:%s\n", client_key_b64);
						mbedtls_sha1(client_key_b64, (size_t) key_len,
								valid_client_key);
						//printf("valid_client_key:%s\n", valid_client_key);
						mbedtls_base64_encode(accept_key, 32, &outlen,
								valid_client_key, 20);
						accept_key[outlen] = 0;
						//printf("accept_key:%s\n", accept_key);
						if (strcmp((char*) accept_key, (char*) server_key)
								!= 0) {
							systemtime();
							ESP_LOGE(TAG,"Invalid websocket key\n");
							err=ESP_FAIL;
							goto END;
						} else {
							systemtime();
							ESP_LOGI(TAG,"Web socket client connected--> %s:%d. Heap:%d",ipv4_str,client->config->localPort,xPortGetFreeHeapSize());
						}

					} //receive is ok.
					free(buffer);
					 client->wsconn=conn;
					 vTaskDelay(1000 / portTICK_PERIOD_MS);
					//Test for Ping if Not going to down connection..
					 ws_client_ping(client);
					 //TickType_t startTime = xTaskGetTickCount();
					 //vTaskDelayUntil(&startTime, 2000 / portTICK_PERIOD_MS);
					 vTaskDelay(1000 / portTICK_PERIOD_MS);
					  //Read from some data is arrive and Check for PON or goto the End
					   ping_respond(client);
					 //vTaskDelay(1000/ portTICK_PERIOD_MS);
					 if (!(xEventGroupGetBits(client->status_bits) & PONANSWER_BIT)) {
						 systemtime();
						 ESP_LOGE(TAG,"Web socket client was connected but not PONG from server.Going to disconnect..:(: %d",portTICK_PERIOD_MS);
						 err=-1;
						 goto END;
					 }
					return err;
				} //write to conn  is ok.
				free(buffer);
			} //Connection was connected.

		}//Connection binding was create.

	}//Connection  was create.

	END:
	if (conn) {
		//systemtime();
		//ESP_LOGE(TAG,"Web socket client netconn_close and netconn_delete connection");
		ESP_LOGE(TAG,"Web socket client netconn_close err:%d",netconn_close(conn));
		ESP_LOGE(TAG,"Web socket client netconn_delete err:%d",netconn_delete(conn));

	}
	//free(buffer);
	return err;
}


//esp_err_t ws_client_pong(esp_ws_client_handle_t client,char* buffer){
//	     err_t  err=ESP_OK;
//		 systemtime();
//		 ESP_LOGI(TAG, "ws_client_pOng.Buffer");
//		 //need to check buffer for some data to retrieve back to the server
//
//		struct netconn *conn=client->wsconn;
//			 if(conn==NULL){
//				 systemtime();
//				 ESP_LOGE(TAG, "ws_write conn is null");
//				 goto END;
//		 }
//		 WS_frame_header_t* p_frame_hdr;
//		 p_frame_hdr=(WS_frame_header_t*) buffer;
//		 uint8_t len=0;
//		 char* p_buf=NULL;
//		 if(p_frame_hdr->payload_length>0){
//			 len=p_frame_hdr->payload_length;
//			  if(p_frame_hdr->mask==0){
//			    char* p_buf=(char*)&buffer[sizeof(WS_frame_header_t)];
//			    ESP_LOGI(TAG, "ws_client_pOng.Buffer:%s",p_buf);
//			  }
//		  }
//
//			 WS_frame_header_t hdr;
//			 hdr.FIN=0x1;
//			 hdr.payload_length=len;
//			 hdr.mask=1;
//			 hdr.reserved=0;
//			 hdr.opcode=WS_OP_PON;
//			 uint32_t key= random_key();
//			 err = netconn_write(conn, &hdr, sizeof(hdr), NETCONN_NOCOPY);
//			 err = netconn_write(conn, &key, sizeof(key), NETCONN_NOCOPY);
//			  if(p_buf!=NULL){
//			    code_data(p_buf,len,key);
//			 err = netconn_write(conn, p_buf, len, NETCONN_NOCOPY);
//			  }
//
//  END:
//return err;
//}




 static err_t ws_read(esp_ws_client_handle_t client){
	 //ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
	err_t err=ERR_OK;
	char *buffer;
	buffer=(char*)calloc(1024,sizeof(char));

	//char buffer[1024];
	struct netconn *conn= client->wsconn;
	if(conn==NULL){
		systemtime();
		ESP_LOGE(TAG, "ws_read conn is null.\n");
		return -1;
	}
	err=recv_task(conn,buffer,1024);             //Waiting from incoming
	if(err ==ERR_OK){
		//systemtime();
       //printf("I have got  some data:,<%s>\n",buffer);
      //Frame header pointer
      	WS_frame_header_t* p_frame_hdr;
      	p_frame_hdr=(WS_frame_header_t*) buffer;
      	if(p_frame_hdr->opcode==WS_OP_CLS){
      		//Close connection
      		err=-1;
      		ESP_LOGE(TAG, "Server want to close connection. err=%d \n",err);
      		goto END;
      	}

      	 if(p_frame_hdr->opcode==WS_OP_PIN){
      	      		//Ping from server
      	      	ESP_LOGE(TAG, "Server want to ping me.");  //eat memory Leak 3000 memory
      	      	 //Need for immediately  answer don't extract in Method !!!
      	        // ws_client_pong(client,buffer); this add delay about 5s.
      	      uint8_t len=0;
      	     		 char* p_buf=NULL;
      	     		 if(p_frame_hdr->payload_length>0){
      	     			 len=p_frame_hdr->payload_length;
      	     			  if(p_frame_hdr->mask==0){
      	     			    p_buf=(char*)&buffer[sizeof(WS_frame_header_t)];
      	     			    ESP_LOGI(TAG, "ws_client_pOng.Buffer:%s",p_buf);
      	     			  }
      	     		  }

      	     			 WS_frame_header_t hdr;
      	     			 hdr.FIN=0x1;
      	     			 hdr.payload_length=len;
      	     			 hdr.mask=1;
      	     			 hdr.reserved=0;
      	     			 hdr.opcode=WS_OP_PON;
      	     			 uint32_t key= random_key();
						 uint8_t  data[sizeof(hdr)+sizeof(key)+len];
						 //uint8_t* pTarget=(uint8_t*)&data;
						 code_data(p_buf,len,key);
                         make_send_buffer((uint8_t*)&data,hdr,key,p_buf,len);
						//  uint8_t* pSource=(uint8_t*)&hdr;
						//   for(int8_t i=0;i<sizeof(hdr);i++)
						//   {
                        //      *pTarget++ =*pSource++; 
						//   }	
						//  pSource=(uint8_t*)&key;
						//    for(int8_t i=0;i<sizeof(key);i++)
						//   {
                        //      *pTarget++ =*pSource++; 
						//   }	
						//   //Masket Message
						//   code_data(p_buf,len,key);
						//   pSource=(uint8_t*)p_buf;
						//    for(int8_t i=0;i<len;i++)
						//   {
                        //      *pTarget++ =*pSource++; 
						//   }	
                           //printf("Pong respond data:")
      	     			 err = netconn_write(conn, &data, sizeof(hdr)+sizeof(key)+len, NETCONN_NOCOPY);
      	     			//  err = netconn_write(conn, &key, sizeof(key), NETCONN_NOCOPY);
      	     			//   if(len>0){
      	     			//     code_data(p_buf,len,key);
      	     			//  err = netconn_write(conn, p_buf, len, NETCONN_NOCOPY);
      	     			//   }
      	       goto END;
          	}

      	  if(p_frame_hdr->opcode==WS_OP_PON){  //pon from server no data received.
      	       printf("PON answer.\n");
      	       xEventGroupSetBits(client->status_bits,PONANSWER_BIT);
      	      	goto END;	        	 //client->serverOK=1;
      	    }


      	//Get payload data must to be lower of WS_STD_LEN
      	   if(p_frame_hdr->payload_length<=WS_STD_LEN){
      		  char* p_buf =(char*)&buffer[sizeof(WS_frame_header_t)];
                   client->event.event_id=WS_EVENT_DATA;
                   client->event.data=p_buf;
                   client->event.data_type=p_frame_hdr->opcode;
                   client->event.data_len=p_frame_hdr->payload_length;
                   esp_ws_send_event(client);
       	    }//Error in WS_STD_LEN
      //free(buffer);
      //send_task(conn,buffer,strlen(buffer));
      //free(buffer);
	}
	  //ESP_ERROR_CHECK(heap_trace_stop());
	  //heap_trace_dump();
	END:
    free(buffer);
	return err;
}

esp_err_t ws_write(esp_ws_client_handle_t client,char *data,int len){
	err_t err=ERR_OK;
	//xSemaphoreTake(client->xSemaphore,portMAX_DELAY);
	struct netconn *conn=client->wsconn;
	 if(conn==NULL){
		 systemtime();
		 ESP_LOGE(TAG, "ws_write conn is null");
		 goto END;
	 }

	 if (client->state == WS_STATE_CONNECTED) {

		//prepare header
		WS_frame_header_t hdr;
		hdr.FIN = 0x1;
		hdr.payload_length = len;
		hdr.mask = 1;
		hdr.reserved = 0;
		hdr.opcode = WS_OP_TXT;
        if(hdr.mask==0) {
		//send header....
		err = netconn_write(conn, &hdr, sizeof(WS_frame_header_t),
				NETCONN_NOCOPY);
		if (err == ESP_OK) {
			err = netconn_write(conn, data, len, NETCONN_NOCOPY);
		 }
        }else{
        	uint32_t key=random_key();
        	code_data(data,len,key);
            uint8_t bufferLen=sizeof(WS_frame_header_t)+sizeof(uint32_t)+len;
		    uint8_t sendBuffer[bufferLen];
		    make_send_buffer((uint8_t*)&sendBuffer,hdr,key,data,len);
	        err = netconn_write(conn, &sendBuffer, bufferLen, NETCONN_NOCOPY);

        	// err = netconn_write(conn, &hdr, sizeof(WS_frame_header_t),NETCONN_NOCOPY);
        	// err = netconn_write(conn, &key, sizeof(key),NETCONN_NOCOPY);
        	// err = netconn_write(conn, data, len,NETCONN_NOCOPY);
        }

		systemtime();
		printf("WS send data'\n");
	} else{
		printf("Can not send data.Web socket server is down...\n");
	}
	END:
	 //xSemaphoreGive(client->xSemaphore);
	return err;
}

esp_err_t ws_client_ping(esp_ws_client_handle_t client)
{
	 err_t  err=ESP_OK;
	 systemtime();
	 ESP_LOGI(TAG, "ws_client_ping..");
	xEventGroupClearBits(client->status_bits, PONANSWER_BIT);
	struct netconn *conn=client->wsconn;
		 if(conn==NULL){
			 systemtime();
			 ESP_LOGE(TAG, "ws_write conn is null");
			 goto END;
		 }else{

	 //prepare header
		 char text[]="hello";
		 uint8_t textLen=strlen(text);
		 WS_frame_header_t hdr;
		 hdr.FIN=0x1;
		 hdr.payload_length=textLen;
		 hdr.mask=1;
		 hdr.reserved=0;
		 hdr.opcode=WS_OP_PIN;
		 uint32_t key= random_key();
		 uint8_t bufferLen=sizeof(WS_frame_header_t)+sizeof(uint32_t)+textLen;
		 uint8_t sendBuffer[bufferLen];
		 code_data(text,textLen,key);
	     make_send_buffer((uint8_t*)&sendBuffer,hdr,key,(char*)&text,textLen);
	     err = netconn_write(conn, &sendBuffer, bufferLen, NETCONN_NOCOPY);

		//  err = netconn_write(conn, &hdr, sizeof(hdr), NETCONN_NOCOPY);
		//  err = netconn_write(conn, &key, sizeof(key), NETCONN_NOCOPY);
		//  err = netconn_write(conn, &text, len, NETCONN_NOCOPY);
		 
		 }

    END:
		return err;
}


static void esp_ws_task_keepalive(void *arg) {
	esp_ws_client_handle_t wsclient = (esp_ws_client_handle_t) arg;
	ESP_LOGI(TAG, "esp_ws_task_keepalive-->create\n");
	static int downCount = 5;  //downCount need on Header or Configured
	xEventGroupClearBits(wsclient->status_bits,SERVERDOWN_BIT);
	xEventGroupClearBits(wsclient->status_bits,KEEPALIVEDOWN);
	while (1) {
		TickType_t startTime = xTaskGetTickCount();
		if(wsclient->status_bits!=NULL){
		if (!(xEventGroupGetBits(wsclient->status_bits) & PONANSWER_BIT)) {
			downCount--;
			ESP_LOGE(TAG, "Keepalive:esp_ws_client ping not PONG from server...!");
			if (downCount <= 0) {
				ESP_LOGE(TAG, "Keepalive:Web Socket Server is Down......!");
				xEventGroupSetBits(wsclient->status_bits,SERVERDOWN_BIT);
				//wsclient->state = WS_STATE_WAIT_TIMEOUT;
                //close(wsclient->wsconn);
			}
		} else {
			downCount = 5;
			xEventGroupClearBits(wsclient->status_bits,SERVERDOWN_BIT);
		}
		if ( xEventGroupGetBits(wsclient->status_bits) & KEEPALIVE_BIT) {
			//ESP_LOGI(TAG, "esp_ws_client ping server..");
			//xEventGroupClearBits(wsclient->status_bits, PONANSWER_BIT);
			ws_client_ping(wsclient);
			vTaskDelayUntil(&startTime,KEEAPALIVETIME / portTICK_PERIOD_MS);
		} else {
			goto END;
		}
	  } else goto END;

	}
	//xEventGroupClearBits(wsclient->status_bits, KEEPALIVE_BIT);
	END:
	ESP_LOGE(TAG, "esp_ws_task_keepalive-->kill\n");
	xEventGroupSetBits(wsclient->status_bits,KEEPALIVEDOWN);
	wsclient->run_ping = 0;
	//free(wsclient); // I am the last who use wsclient
	vTaskDelete(NULL);
}

static void esp_ws_task(void *arg) {
	systemtime();
	ESP_LOGI(TAG, "esp_ws_task run.Heap:%d", xPortGetFreeHeapSize());
	esp_ws_client_handle_t client = (esp_ws_client_handle_t) arg;
	xEventGroupSetBits(client->status_bits, ESPWSTASKRUN); //client->run = true;
	client->state = WS_STATE_INIT;
	xEventGroupClearBits(client->status_bits, STOPPED_BIT); //Going to run ws client
	static err_t err;
	while (ESPWSTASKRUN & xEventGroupGetBits(client->status_bits)) { //client->run
		switch ((int) client->state) {
		case WS_STATE_INIT:
			//ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
			err = esp_ws_connect(client);
			//ESP_ERROR_CHECK(heap_trace_stop());
			//heap_trace_dump();
			if (err != ESP_OK) {
				printf("WS_STATE_INIT -->last_err:%d\n", err);
				switch (err) {
				case -11:  //ERR_ABRT
					break;
				case -4:   //ERR_RTE
					goto END;
					break;
				case -8:    //Address is in used

				  vTaskDelay(50000 / portTICK_PERIOD_MS); //50s
				  break;
				case -12: //ERR_RST
					vTaskDelay(5000 / portTICK_PERIOD_MS);
					break;
				default:
					vTaskDelay(5000 / portTICK_PERIOD_MS);
					break;
				}
				systemtime();
				ESP_LOGI(TAG, "There are no Web Socket Server.");
				if (client->config->reconnect) { 	//if  reconnect try it now
					//vTaskDelay(5000/portTICK_PERIOD_MS );
					systemtime();
					ESP_LOGI(TAG, "Try it now.");
					break;
				} else {
					systemtime();
					ESP_LOGI(TAG, "I am going to home.");
					xEventGroupClearBits(client->status_bits, ESPWSTASKRUN); //client->run = false;
					break;
				}
			}
			client->event.event_id = WS_EVENT_CONNECTED;
			esp_ws_send_event(client);
			client->state = WS_STATE_CONNECTED;
			break;

		case WS_STATE_CONNECTED:

			//Create ping schedule task..
			if (!client->run_ping)
			{
				client->run_ping = 1;
				xEventGroupSetBits(client->status_bits, KEEPALIVE_BIT);
				xEventGroupSetBits(client->status_bits, PONANSWER_BIT);
				err = xTaskCreate(esp_ws_task_keepalive, "esp_ws_task_keepalive", 2048, client, 5, NULL);
				if (err != pdTRUE)
				{
					ESP_LOGE(TAG, "Error create esp_ws_task_keepalive..\n");
					client->run_ping = 0;
					goto END;
				}
			}
			err = ws_read(client);
			if (err != ERR_OK) {
				 if(err==ERR_TIMEOUT){
					 //Check for ping out from Server 
					if (!(xEventGroupGetBits(client->status_bits) & SERVERDOWN_BIT)){
                      //Going now to read
					   break;
					}
					
				 }
				systemtime();
				ESP_LOGE(TAG, "Web socket server is DOWN !!! err:%d",err);
				client->event.event_id = WS_EVENT_DISCONNECTED;
				esp_ws_send_event(client);
				if (client->config->reconnect) {
					systemtime();
					ESP_LOGI(TAG, "I am going to reconnect... clear wsconn-->heap:%d",xPortGetFreeHeapSize());
					if (client->wsconn) {
						netconn_close(client->wsconn);
						netconn_delete(client->wsconn);
						//free(client->wsconn);
						client->wsconn = NULL;
						ESP_LOGI(TAG, "I am going to reconnect... wsconn was cleared -->heap:%d",xPortGetFreeHeapSize());
					}
					    xEventGroupClearBits(client->status_bits, KEEPALIVE_BIT);
					    //Wait keepalive to stop..
					    xEventGroupWaitBits(client->status_bits,KEEPALIVEDOWN,false,true,portMAX_DELAY);
					    systemtime();
					 	ESP_LOGI(TAG, "I am going to reconnect --> keepalive was killed.\n");

					client->state = WS_STATE_INIT;
					break;
				} else {
					systemtime();
					ESP_LOGI(TAG, "I am going to home...");
					xEventGroupClearBits(client->status_bits,ESPWSTASKRUN);//client->run = false;
					break;
				}
			}

			break;
		case WS_STATE_WAIT_TIMEOUT:
		     ESP_LOGE(TAG,"From WS_STATE_WAIT_TIMEOUT Websocket Server is Down.");
		    //goto END; // Not good going to reboot 
			break;
		default:
			break;
		}					//End Switch
	}					//End While(1)
	END:
	xEventGroupClearBits(client->status_bits, KEEPALIVE_BIT);
	if(check_task("esp_ws_task_keepalive")){
		systemtime();
		ESP_LOGE(TAG, "esp_ws_task killed waiting for kill keepalive..!\n");
		xEventGroupWaitBits(client->status_bits,KEEPALIVEDOWN,false,true,portMAX_DELAY);
	    }
	systemtime();
	ESP_LOGE(TAG, "esp_ws_task was killed..! Heap:%d",xPortGetFreeHeapSize());
	client->event.event_id = WS_EVENT_DISCONNECTED;
	esp_ws_send_event(client);
	if (client->wsconn) {
			//xSemaphoreTake(client->xSemaphore,portMAX_DELAY);
			netconn_close(client->wsconn);
			netconn_delete(client->wsconn);
			client->wsconn = NULL;
			vEventGroupDelete(client->status_bits);
			free(client);
			//xSemaphoreGive(client->xSemaphore);
		}

	vTaskDelete(NULL);
}

static err_t esp_ms_client_destroy(esp_ws_client_handle_t client){
	err_t err=ERR_OK;
//	ws_client_config_t *cfg=client.config;
//	free(cfg->serverIP);
//	free(cfg->localPort);

	vEventGroupDelete(client->status_bits);
    free(client);
	return err;

}

esp_ws_client_handle_t esp_ws_client_init(const esp_ws_client_config_t *config) {

	esp_ws_client_handle_t client = calloc(1, sizeof(struct esp_ws_client));
	//client->wsconn=&wsconn;
	//client =calloc(1,sizeof(struct conn));
	//client =calloc(1,sizeof(struct ws_client_state_t));
	//Only for information
    //printf("Size of esp_ws_client:%d\n",sizeof(struct esp_ws_client));
    //printf("Size of netconn:%d\n",sizeof(struct netconn));
    //printf("Size of ws_client_state_t:%d\n",sizeof(ws_client_state_t));
    //printf("Size of ws_client_config_t:%d\n",sizeof(ws_client_config_t));

	ESP_MEM_CHECK(TAG, client, return NULL);
	esp_ws_set_config(client, config);
	client->status_bits = xEventGroupCreate();
	//client->xSemaphore= xSemaphoreCreateMutex();
	ESP_MEM_CHECK(TAG, client->status_bits, goto _ms_init_failed);
	return client;

	_ms_init_failed:
	printf("esp_ws_client_init -- failed.\n");
	esp_ms_client_destroy(client);
	return NULL;
}

esp_err_t esp_ws_client_start(esp_ws_client_handle_t client ){
	err_t err=ESP_OK;
    if(check_task("esp_ws_task")){
    	ESP_LOGE(TAG,"WebSocket client already is started !!!");
    	return ESP_FAIL;
    }

//	do{
//		//waiting for kill previous task
//		systemtime();
//		printf("waiting for kill previous esp_ws_task: %d \n",check_task("esp_ws_task"));
//		//xEventGroupClearBits(client->status_bits,ESPWSTASKRUN);//client->run=false;
//		vTaskDelay(1000 / portTICK_PERIOD_MS);
//	}while(check_task("esp_ws_task"));


	//Create task esp_ws_task
    if(xTaskCreate(esp_ws_task,"esp_ws_task",3072,client,5,NULL)!=pdTRUE){
       ESP_LOGE(TAG, "Error create WebSocket task");
   	   return ESP_FAIL;
    }

	return err;
}

esp_err_t esp_ws_client_stop(esp_ws_client_handle_t client)
{
	xEventGroupClearBits(client->status_bits,ESPWSTASKRUN);//client->run = false;
    xEventGroupWaitBits(client->status_bits, STOPPED_BIT, false, true, portMAX_DELAY);
    client->state = WS_STATE_UNKNOWN;
    return ESP_OK;
}



