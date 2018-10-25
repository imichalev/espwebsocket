/*
 * MAX6675K.c
 *
 *  Created on: 12.07.2018 ã.
 *      Author: imihalev
 */

#include "string.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_system.h"
#include "driver/spi_master.h"

#include "MAX31855.h"

static spi_device_handle_t spi;
//static bool init;

static esp_err_t  init_spi() {
	printf("Init_spi_bus....\n");
	esp_err_t err;
	//SPI Must be configure:
	gpio_set_direction(PIN_NUM_CS_MAX,GPIO_MODE_OUTPUT);
	gpio_set_direction(PIN_NUM_CLK_MAX,GPIO_MODE_OUTPUT);
	gpio_set_direction(PIN_NUM_MISO_MAX,GPIO_MODE_INPUT);

	spi_bus_config_t buscfg = {
			.miso_io_num = PIN_NUM_MISO_MAX,
			.sclk_io_num = PIN_NUM_CLK_MAX,
			.mosi_io_num = -1,
			.quadwp_io_num = -1,
			.quadhd_io_num = -1,
	};

	err=spi_bus_initialize(HSPI_HOST,&buscfg,0);
		if(err !=ESP_OK){
		  goto out;
		}

	spi_device_interface_config_t devcfg = {
			.clock_speed_hz = 100000,
			.mode =0,
			.spics_io_num = PIN_NUM_CS_MAX,
			.queue_size = 1,
			.dummy_bits=0,

	};

	err=spi_bus_add_device(HSPI_HOST,&devcfg,&spi);
	if(err !=ESP_OK){
          goto out;
		}
	//init=true;
	return ESP_OK;
out:
    printf("error in spi init....\n");
    //init=false;
    spi=NULL;
	return err;
}

static esp_err_t  deinit_spi(){

	esp_err_t err=0;
	 if(spi!=NULL){
	 printf("deinit max31855 spi ....\n");
	spi_bus_remove_device(spi);
	spi_bus_free(HSPI_HOST);
    spi=NULL;
	 }
   return err;
}


//uint32_t read_max_data_lobo(){
//	uint32_t data=0;
//	esp_err_t err=0;;
//   if(max_spi!=NULL){
//	 err = spi_lobo_device_select(max_spi, 1);
//	 assert(err==ESP_OK);
//	  printf("ready to read max.....\n");
//	 err = spi_lobo_device_deselect(max_spi);
//	 assert(err==ESP_OK);
//	 printf("deselect  max from spi bus .....\n");
//
//   }
//  return data;
//}




uint32_t read_max_data(){
	if(spi==NULL){
		init_spi();
	}
	spi_transaction_t trans_desc;
	esp_err_t err;
    //uint32_t data;
    //uint8_t rx_data[4];
    bzero(&trans_desc,sizeof(trans_desc));
    trans_desc.flags |=SPI_TRANS_USE_RXDATA;
    trans_desc.length=32;
    //trans_desc.rxlength=32;
    //trans_desc.rx_buffer=&data;


    err=spi_device_transmit(spi,&trans_desc);
    if(err!=ESP_OK){
      goto out;
    }

//   printf("max data_0: %x\n",trans_desc.rx_data[0]);
//   printf("max data_1: %x\n",trans_desc.rx_data[1]);
//   printf("max data_2: %x\n",trans_desc.rx_data[2]);
//   printf("max data_3: %x\n",trans_desc.rx_data[3]);



   //deinit_spi();

    return (trans_desc.rx_data[0]<<24)|(trans_desc.rx_data[1]<<16)|(trans_desc.rx_data[2]<<8)|trans_desc.rx_data[3];
//    return ((data>>24)&0xff) | // move byte 3 to byte 0
//    		((data<<8)&0xff0000) | // move byte 1 to byte 2
//    		((data>>8)&0xff00) | // move byte 2 to byte 1
//    		 ((data<<24)&0xff000000); // byte 0 to byte 3


    out:
	   deinit_spi();
        printf("error in spi init....\n");
    	return err;
}

