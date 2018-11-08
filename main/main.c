#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <sys/time.h>
//#include "time.h"
#include "string.h"
#include "si7021.h"
#include "esp_heap_trace.h"
#include "lwip/api.h"

//Test TFT
//#include "/ESP32_TFT_library/components/tft/tft.h"
#include "tft.h"
#include "tftspi.h"
#include "spi_master_lobo.h"
 //For M5stack
//#define	CONFIG_EXAMPLE_DISPLAY_TYPE 3

#include "ws_client.h"

#include "ds3231.h"
//#include "MAX31855.h"
#include "MAX6675K.h"
#include "termo_controller.h"



static const char *TAGWIFI="Wifi_Me";
static const char *TAGTASK="TASK";
static  esp_ws_client_handle_t wsclient=NULL;
//static  struct  esp_ws_client *wsclient;
//static struct esp_ws_client client;
//#define 	RAND_MAX  100

#define NUM_RECORDS 100
#define SPI_BUS TFT_VSPI_HOST
#define MAX_TEMPERATURE 26.00

unsigned short usStackHighWaterMark;
static  heap_trace_record_t trace_record[NUM_RECORDS];

static void taskInfo(void){

	TaskStatus_t *pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;
	uint32_t ulTotalRunTime;//, ulStatsAsPercentage;
	//ulTotalRunTime=0UL;
	//*pcWriteBuffer = 0x00;
	uxArraySize = uxTaskGetNumberOfTasks();
	ESP_LOGI(TAGTASK, "Number of task:%d",uxArraySize);
	pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );
	//ESP_LOGI(TAGTASK, "Size of pxTaskStatusArray:%d",sizeof(pxTaskStatusArray));
	if( pxTaskStatusArray != NULL )
	 {
	     // Generate raw status information about each task.
	     uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime );

	     // For percentage calculations.
	     //ESP_LOGI(TAGTASK, "ulTotalRunTime:%lu",ulTotalRunTime);
	     ulTotalRunTime /= 100UL;
	    // ESP_LOGI(TAGTASK, "ulTotalRunTime:%lu",ulTotalRunTime);
	     // Avoid divide by zero errors.
	     if( ulTotalRunTime == 0 )
	     {
	    	// printf("ulTotalRunTime:%lu\n",ulTotalRunTime);
	         // For each populated position in the pxTaskStatusArray array,
	         // format the raw data as human readable ASCII data
	         for( x = 0; x < uxArraySize; x++ )
	         {
	        	 //printf("uxArraySize:%lu\n",x);
	             // What percentage of the total run time has the task used?
	             // This will always be rounded down to the nearest integer.
	             // ulTotalRunTimeDiv100 has already been divided by 100.

	             //ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalRunTime;

                 if(x%2==0){
                   printf("%02d.%-15s:%d\r\n",(int)x, pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].usStackHighWaterMark);
                 }else{
	        	 printf("%02d.%-15s:%d\r\n",(int)x, pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].usStackHighWaterMark);
                 }

//	             if( ulStatsAsPercentage > 0UL )
//	             {
//	            	 //printf("ulStatsAsPercentage:%lu\n",ulStatsAsPercentage);
//	                 //sprintf( pcWriteBuffer, "%s\t\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
//	                 printf("%s\t\t%d\t\t%d\t\t%lu%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter, pxTaskStatusArray[ x ].usStackHighWaterMark, ulStatsAsPercentage );
//
//	             }
//	             else
//	             {
//	                 // If the percentage is zero here then the task has
//	                 // consumed less than 1% of the total run time.
//	                 //sprintf( pcWriteBuffer, "%s\t\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter );
//	            	 printf("%s\t\t%d\t\t<1%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter );
//	             }

	             //pcWriteBuffer += strlen( ( char * ) pcWriteBuffer );
	         }
	     }

	     // The array is no longer needed, free the memory it consumes.
	     vPortFree( pxTaskStatusArray );
	 }else{
		 printf("TaskInfo-->pxTaskStatusArray is NULL.\n");
	 }


}


//static void display_data(char* charData,uint8_t row, ){
//
//
//
//
//}


static void task_sensor() {
	static double temperatureMax=0.0;
	static double temperatureMin=30.0;
	double temperature = -100L;
	double humidity = -100L;
	//char *charData;
	//charData=(char*)calloc(20,sizeof(char));  //CORRUPT HEAP: multi_heap.c:308 detected at 0x3ffcb0f8

	char charData[25];
	//char charDataHumidity[15];
	//bzero(charData,20);

	bool si7021ok = 0;
	char tftDisplay[25];
	//bzero(tftDisplay,20);
	TFT_setFont(DEJAVU24_FONT, NULL);
	if (readTemperature(&temperature) == ESP_OK) {
		si7021ok = 1;
		if(temperatureMax<temperature){
			temperatureMax=temperature;
		}
		if(temperatureMin>temperature){
			temperatureMin=temperature;
		}
		//sprintf(charDataTemperature, "{\"t\":%6.2f}", temperature);
		  //Send it if websocket connect
		// if (wsclient != NULL)
		// {
		// 	if (wsclient->state == WS_STATE_CONNECTED)
		// 	{
		// 		if (si7021ok)
		// 		{
		// 			//ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
		// 			//printf("Try to send data via websocket...\n");
		// 			//sprintf(charData, "{\"t\":%6.2f}", temp);
		// 			ws_write(wsclient, charData, strlen(charData));
		// 			//ws_write(wsclient, charHumidity, strlen(charHumidity));
		// 			//ESP_ERROR_CHECK(heap_trace_stop());
		// 			//heap_trace_dump();
		// 		}
		// 	}
		// }
		sprintf(tftDisplay, "Temperature:%6.2f",temperature);
		_fg = TFT_GREEN;
		 if(temperature>MAX_TEMPERATURE){
			 _fg = TFT_RED;
		 }
		TFT_print(tftDisplay,CENTER,3*(TFT_getfontheight()+2));


		sprintf(tftDisplay, "%6.2f <--> %6.2f",temperatureMin,temperatureMax);
		_fg = TFT_WHITE;
		TFT_print("\r",0,4*(TFT_getfontheight()+2));
		TFT_print(tftDisplay,CENTER,4*(TFT_getfontheight()+2));


	}
	//printf("1.charData:%s and charDataLen:%d\n",charData,strlen(charData));
	si7021ok = 0;
	if (readHumidity(&humidity) == ESP_OK) {
		//sprintf(charDataHumidity, "{\"h\":%6.2f}",humidity);
		si7021ok = 1;
		sprintf(tftDisplay, "Humidity:%6.2f", humidity);
		_fg = TFT_ORANGE;
		TFT_print(tftDisplay,CENTER,5*(TFT_getfontheight()+2));

	}
	TFT_setFont(DEFAULT_FONT, NULL);
	//taskInfo();
	//printf("2. charData:%s and charDataLen:%d\n",charData,strlen(charData));
	if (wsclient != NULL) {
		if (wsclient->state == WS_STATE_CONNECTED) {
			if (si7021ok) {
				 sprintf(charData, "{\"t\":%6.2f,", temperature);	
			     sprintf(charData, "%s\"h\":%6.2f}",charData,humidity);	  
				  			
				//ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
				//printf("Try to send data via websocket...\n");
				//sprintf(charData, "{\"t\":%6.2f}", temp);		
				ws_write(wsclient, charData, strlen(charData));
				//ws_write(wsclient, charHumidity, strlen(charHumidity));
				//ESP_ERROR_CHECK(heap_trace_stop());
				//heap_trace_dump();
			}
		}
	}
	 //free(charData);=
}

static void task_sensor_max_6675(){
	max6675k_data_t* data;
	uint16_t max=read_max_6675_data();
	data=((max6675k_data_t*) &max);

	static int status=0;
//	printf("Data from max sensor              :%x\n",max);
//	printf("Data from max sensor sign         :%x\n",data->sign);
//	printf("Data from max sensor temp         :%x\n",data->temp);
//	printf("Data from max sensor thermocouple :%x\n",data->thermocouple);
//	printf("Data from max sensor id           :%x\n",data->id);
//	printf("Data from max sensor fault    :%x\n",data->fault);
//	printf("Data from max sensor scvfault :%x\n",data->scvfault);
//	printf("Data from max sensor sgvfault :%x\n",data->scgfault);
//	printf("Data from max sensor ocfault  :%x\n",data->ocfault);

    

    TFT_setFont(DEJAVU18_FONT, NULL);
    static size_t strLen=0;
	if(!data->thermocouple){

	    float temp=((float)data->temp * 0.25);
		char temperature [25];


		sprintf(temperature,"Temperature is:%6.2f",temp);
		if(strLen!=strlen(temperature))//Clear display if last row is different;
		{
			status=0;
			strLen=strlen(temperature);
		}
		if(status!=1){
		 TFT_print("\r",0,3*(TFT_getfontheight()+2)); //clear line
		 status=1;
		}


		//printf("Temperature is :%6.2f\n",temp);

		_fg = TFT_GREEN;
		 if(temp>26){
        _fg = TFT_RED;
		}
		if( temp<10){
		 _fg = TFT_BLUE;
		}

        TFT_print(temperature,CENTER,3*(TFT_getfontheight()+2));
		//Send websocket Server ....
		if (wsclient != NULL)
		{
			if (wsclient->state == WS_STATE_CONNECTED)
			{
				
					//ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
					//printf("Try to send data via websocket...\n");
					char charData[11];
					//Make JSON FORMAT for data !!! in server is good to parse data :)....
					sprintf(charData, "{\"t\":%6.2f}", temp);				
					ws_write(wsclient, charData, strlen(charData));
					//ws_write(wsclient, charHumidity, strlen(charHumidity));
					//ESP_ERROR_CHECK(heap_trace_stop());
					//heap_trace_dump();
				
			}
	}




	}else{
	  	  if(status!=3){
		  TFT_print("\r",0,3*(TFT_getfontheight()+2)); //clear line
		  status=3;
	      }
	     _fg = TFT_ORANGE;
		 TFT_print("Error !!!",CENTER,3*(TFT_getfontheight()+2));
	}

    TFT_setFont(DEFAULT_FONT, NULL);

	
	
}

//static void task_sensor_max_31855(){
//	max31855_data_t* data;
//	uint32_t max=read_max_data();
//	data=((max31855_data_t*) &max);
//	static int status=0;
////	printf("Data from max sensor          :%x\n",max);
////	printf("Data from max sensor tempouts :%x\n",data->tempouts);
////	printf("Data from max sensor tempout  :%x\n",data->tempout);
////	printf("Data from max sensor tempins  :%x\n",data->tempins);
////	printf("Data from max sensor tempin   :%x\n",data->tempin);
////	printf("Data from max sensor fault    :%x\n",data->fault);
////	printf("Data from max sensor scvfault :%x\n",data->scvfault);
////	printf("Data from max sensor sgvfault :%x\n",data->scgfault);
////	printf("Data from max sensor ocfault  :%x\n",data->ocfault);
//
//    TFT_setFont(DEJAVU18_FONT, NULL);
//
//	if(!data->fault){
//
//	    float temp=((float)data->tempout * 0.25);
//		char temperature [25];
//
//		if(!data->tempouts){
//		sprintf(temperature,"Temperature is :%6.2f\n",temp);
//		if(status!=1){
//		 TFT_print("\r",0,3*(TFT_getfontheight()+2)); //clear line
//		 status=1;
//		}
//		}else{
//		sprintf(temperature,"Temperature is :-%6.2f\n",(2048.25-temp));
//		if(status!=2){
//		  TFT_print("\r",0,3*(TFT_getfontheight()+2)); //clear line
//		  status=2;
//	      }
//		}
//		//printf("Temperature is :%6.2f\n",temp);
//
//		_fg = TFT_GREEN;
//		 if(temp>26){
//        _fg = TFT_RED;
//		}
//		if( (data->tempouts) || (temp<10)){
//		 _fg = TFT_BLUE;
//		}
//
//        TFT_print(temperature,CENTER,3*(TFT_getfontheight()+2));
//
//	}else{
//	  	  if(status!=3){
//		  TFT_print("\r",0,3*(TFT_getfontheight()+2)); //clear line
//		  status=3;
//	      }
//	     _fg = TFT_ORANGE;
//		 TFT_print("Error",CENTER,3*(TFT_getfontheight()+2));
//	}
//
//    TFT_setFont(DEFAULT_FONT, NULL);
//}

static esp_err_t ws_event_handler(esp_ws_event_handle_t event){
	esp_err_t err=ESP_OK;
	//char data_read[255];
	char* data_read;
	data_read =	calloc(255,sizeof(char));
	//bzero(data_read, 255);
        switch(event->event_id){
         case WS_EVENT_CONNECTED:
        	 printf("Web Socket client connected to server.\n");
        	//TFT_clearStringRect(CENTER, 2*(TFT_getfontheight()+2), "Web Socket client connected!");
            TFT_print("\r",0,2*(TFT_getfontheight()+2));
        	_fg = TFT_GREEN;
        	TFT_print("Web Socket client connected!",CENTER,2*(TFT_getfontheight()+2));
        	 break;
         case WS_EVENT_DISCONNECTED:
        	 printf("Web Socket client disconnected from server.\n");
        	 //TFT_clearStringRect(CENTER, 2*(TFT_getfontheight()+2), "Web Socket client disconnected!");
        	 TFT_print("\r",0,2*(TFT_getfontheight()+2));
        	 _fg = TFT_RED;
        	 TFT_print("Web Socket client disconnected!",CENTER,2*(TFT_getfontheight()+2));
             break;
         case WS_EVENT_DATA:
        	 //printf("Web Socket client data received..\n");
        	 memcpy(data_read,event->data,event->data_len);
        	 if(event->data_type==WS_OP_TXT){
        	 printf("Web Socket client data received in TXT format:%s\n",data_read);
        	   if(strstr(data_read,"Hello you are  welcomme")){ //Welcomme message...
        		   vTaskDelay(1000 /portTICK_PERIOD_MS);
        		   task_sensor();
        	   }
        	 }
             break;

         case WS_EVENT_ERROR:
           	 printf("Web Socket client error..\n");
             break;

        }
       free(data_read);
	return ESP_OK;
}

static void ws_init() {


	if (wsclient == NULL) {   //Only first time config or is wsclient was destroyed
		const esp_ws_client_config_t ws_client_cfg = {
			    //.serverIP="192.168.1.205",
				.serverIP ="10.240.63.218",  //211
				.serverPort = 999,           //8080
				.localPort = (rand()& 0x7FFF) + 10000,
				.reconnect = 1,
				.event_handle =	ws_event_handler };

		//printf("Size of ws_client before:%d\n",sizeof(*wsclient));
		wsclient = esp_ws_client_init(&ws_client_cfg);
	}
   //printf("Size of ws_client after:%d\n",sizeof(*wsclient));
  esp_ws_client_start(wsclient);

}

esp_err_t event_handler(void *ctx, system_event_t *event) {
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		ESP_LOGI(TAGWIFI, "I am going to connect !!!");
		break;

	case SYSTEM_EVENT_STA_GOT_IP:
		ESP_LOGI(TAGWIFI, "got ip:%s",
				ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
		//Here call web socket
		   ws_init();
		break;

	case SYSTEM_EVENT_AP_STACONNECTED:
		ESP_LOGI(TAGWIFI, "station:"MACSTR" join, AID=%d",
				MAC2STR(event->event_info.sta_connected.mac),
				event->event_info.sta_connected.aid);
		break;

	case SYSTEM_EVENT_AP_STADISCONNECTED:
		ESP_LOGI(TAGWIFI, "station:"MACSTR"leave, AID=%d",
				MAC2STR(event->event_info.sta_disconnected.mac),
				event->event_info.sta_disconnected.aid);
		break;

	case SYSTEM_EVENT_STA_DISCONNECTED:
		ESP_LOGI(TAGWIFI, "STA is Down :( !!!");
		 vTaskDelay(1000/portTICK_PERIOD_MS );
//         if(wsclient->wsconn!=NULL){
//        	 netconn_close(wsclient->wsconn);
//        	 netconn_delete(wsclient->wsconn);
//         }
//		 vTaskDelay(1000/portTICK_PERIOD_MS );
//		 ESP_ERROR_CHECK( esp_wifi_start() );
		 ESP_ERROR_CHECK(esp_wifi_connect());
		break;

	case SYSTEM_EVENT_WIFI_READY:
		ESP_LOGI(TAGWIFI, "WIFI READY :) !!!");
		break;

	case SYSTEM_EVENT_AP_STOP:
		ESP_LOGI(TAGWIFI, "AP is DOWN :( ... !");
		break;
	case SYSTEM_EVENT_SCAN_DONE:
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		ESP_LOGI(TAGWIFI, "STA CONNECTED....");
		break;

	case SYSTEM_EVENT_AP_START:
		ESP_LOGI(TAGWIFI, "AP START...");
		break;

	//case SYSTEM_EVENT_AP_STAIPASSIGNED:
	//	ESP_LOGI(TAGWIFI, "AP STAIPASSIGNED...");
	//	break;

	default:
		ESP_LOGI(TAGWIFI, "IMPORTAN EVENT SOMETHINGS IS WRONG !....:%d",
				event->event_id);
		//wifiStatus.init=0;
		break;
	}
	return ESP_OK;
}

static void chek_local_time() {
	time_t now;
	time(&now);
	time_t ds3231 = read_ds3231_time();
	if (ds3231 != now) {
		struct timeval tv;
		tv.tv_sec = ds3231;
		tv.tv_usec = 290944;
		settimeofday(&tv, NULL);
	}
}

void init_tft() {
	//Test TFT



	// ==== Set display type =====

      /**
	  * for DISP_TYPE_ILI9341
	  */
	 //tft_disp_type = DISP_TYPE_ILI9341; //comment for M5 and set display type in tftspi.h <#define  CONFIG_EXAMPLE_DISPLAY_TYPE 3>




	 /**
	  * for M5 Display
	  */
	 //#define  CONFIG_EXAMPLE_DISPLAY_TYPE 3  
	_width = DEFAULT_TFT_DISPLAY_WIDTH;
	_height = DEFAULT_TFT_DISPLAY_HEIGHT;
	// ==== Set maximum spi clock for display read    ====
	//      operations, function 'find_rd_speed()'    ====
	//      can be used after display initialization  ====
	max_rdclock = 8000000;
	// ===================================================
	// ====================================================================
	// === Pins MUST be initialized before SPI interface initialization ===
	// ====================================================================
	TFT_PinsInit();
	// ====  CONFIGURE SPI DEVICES(s)  ====================================================================================
	spi_lobo_device_handle_t spi;
	printf("PIN_NUM_MISO:%d\nPIN_NUM_MOSI:%d\nPIN_NUM_CLK:%d\nPIN_NUM_CS:%d\nPIN_NUM_DC:%d\nPIN_NUM_TCS:%d\nPIN_NUM_RST:%d\nPIN_NUM_BCKL:%d\n"
			,PIN_NUM_MISO
			,PIN_NUM_MOSI
			,PIN_NUM_CLK
			,PIN_NUM_CS
			,PIN_NUM_DC
			,PIN_NUM_TCS
			,PIN_NUM_RST
	        ,PIN_NUM_BCKL
	  );
	spi_lobo_bus_config_t buscfg = {
			.miso_io_num = PIN_NUM_MISO, // set SPI MISO pin
			.mosi_io_num = PIN_NUM_MOSI, // set SPI MOSI pin
			.sclk_io_num = PIN_NUM_CLK, // set SPI CLK pin
			.quadwp_io_num = -1,
			.quadhd_io_num = -1,
			.max_transfer_sz = 6* 1024
	 };
	spi_lobo_device_interface_config_t devcfg = {
			.clock_speed_hz = 8000000,          // Initial clock out at 8 MHz
			.mode = 0,                          // SPI mode 0
			.spics_io_num = -1,                 // we will use external CS pin
			.spics_ext_io_num = PIN_NUM_CS,     // external CS pin
			.flags = LB_SPI_DEVICE_HALFDUPLEX 	// ALWAYS SET  to HALF DUPLEX MODE!! for display spi
	};

	// ==================================================
	// ==== Initialize the SPI bus and attach the LCD to the SPI bus ====
	printf("Try to init SPI_BUS\n");
	esp_err_t err = spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
	//assert(err==ESP_OK);
	if (err == ESP_OK) {
		printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
	} else {
		printf("Init SPI_BUS:(%d) failed....\n", SPI_BUS);
	}
	disp_spi = spi;
	// ==== Test select/deselect ====
	err = spi_lobo_device_select(spi, 1);
	assert(err==ESP_OK);
	err = spi_lobo_device_deselect(spi);
	assert(err==ESP_OK);
	printf("SPI: attached display device, speed=%u\r\n",
			spi_lobo_get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n",
			spi_lobo_uses_native_pins(spi) ? "true" : "false");
	// ==== Initialize the Display ====
	printf("SPI: display init...\r\n");
	TFT_display_init();
	printf("OK\r\n");
	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
	TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(LANDSCAPE);
	TFT_resetclipwin();
	TFT_setFont(DEJAVU18_FONT, NULL);
	_fg = TFT_MAGENTA;
	TFT_print("ALABALA I am Here !!!", CENTER, CENTER);
	TFT_setFont(DEFAULT_FONT, NULL);

//	//add max31855
//	 spi_lobo_device_handle_t maxspi = NULL;
//	 spi_lobo_device_interface_config_t maxdevcfg={
//	         .clock_speed_hz=100000,                //Clock out at 100000khz
//	         .mode=0,                                //SPI mode 0
//	         .spics_ext_io_num=-1,                   //Not using the external CS
//	 		 .spics_io_num = PIN_NUM_CS_MAX,
//	      };
//	 err=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &maxdevcfg, &maxspi);
//	 assert(err==ESP_OK);
//	 max_spi=maxspi;
//	 err = spi_lobo_device_select(maxspi, 1);
//	 assert(err==ESP_OK);
//	 err = spi_lobo_device_deselect(maxspi);
//	 assert(err==ESP_OK);

}

void app_main(void)
{
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record,NUM_RECORDS));
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config = {
        .sta = {
          //.ssid ="Michalev",
		  // .password="mich@l40",


            .ssid = "MikroTik-CEDA81",
            .password = "zaq12wsx!",
            .bssid_set = false

        }
    };
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );

	init_tft();

    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    int level = 1;
	time_t now;
	static time_t taskTime;
	time(&now);
	taskTime=now;
	struct tm timeinfo;
	localtime_r(&now, &timeinfo);
	char datetime[30];
	printf("Starting:%02d:%02d:%02d\n",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
	user_register_t  usr[1];
	if(readUserRegister(usr)==ESP_OK){
		printf("SI7021 Resolution configured:%d%d.\n",usr->RES1,usr->RES0);
		if(usr->RES1 || usr->RES0){ //Check and set to max Measurement Resolution
			usr->RES1=0;
			usr->RES0=0;
			printf("SI7021 Write to max Resolution.\n");
			writeUserRegister(usr); //Set to max resolution
		}
	}
	//free(usr);

	chek_local_time();

	while (true) {

		//printf("%f",temperature);
		time(&now);
		localtime_r(&now, &timeinfo);
		sprintf(datetime, "%02d:%02d:%02d free heap:%d\n", timeinfo.tm_hour,
							timeinfo.tm_min, timeinfo.tm_sec,xPortGetFreeHeapSize());
		_fg = TFT_CYAN;
		TFT_print(datetime,CENTER,TFT_getfontheight()+2);
        
		if (now-taskTime > 60 ) {    // 60 - one minute timeinfo.tm_sec % 50 == 0
			time(&taskTime);
			
			//taskInfo();
			//printf("%s", datetime);//system_get_free_heap_size());
               //task_sensor();
			   task_sensor_max_6675();
			 //free();
			  //time(&now);
			  //printf("Test random:%lu\n",now);
			  //printf("Test random:%d\n",((int)now &0x7FFF)+10000);
		} //30 seconds

//		if (timeinfo.tm_sec % 3 == 0) {
//
//					controllerOn();
//
//				};


		gpio_set_level(GPIO_NUM_2, level);
		level = !level;
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

