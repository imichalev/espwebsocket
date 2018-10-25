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
#include "freertos/task.h"
#include "MAX6675K.h"

static spi_device_handle_t spi;
//static bool init;

static esp_err_t  init_spi() {
	printf("Init_spi_bus....\n");
	esp_err_t err;
	spi_bus_config_t buscfg = {
			.miso_io_num = PIN_NUM_MISO_MAX,
			.sclk_io_num = PIN_NUM_CLK_MAX,
			.mosi_io_num = -1,
			.quadwp_io_num = -1,
			.quadhd_io_num = -1,
	};

	err=spi_bus_initialize(HSPI_HOST,&buscfg,2);
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




uint16_t read_max_6675_data(){
	if(spi==NULL){
		init_spi();
		//It is for first time read need to some delay for calibration MAX6675K
		vTaskDelay(1000);
	}
	spi_transaction_t trans_desc;
	esp_err_t err;
    bzero(&trans_desc,sizeof(trans_desc));
    trans_desc.flags |=SPI_TRANS_USE_RXDATA;
    trans_desc.length=16;
    err=spi_device_transmit(spi,&trans_desc);
    if(err!=ESP_OK){
      goto out;
    }

   //printf("max data_0: %x\n",rawtemp[0]);
   //printf("max data_1: %x\n",rawtemp[1]);

    if(trans_desc.rx_data[1]&0x4){
    	printf("max no termo_coupler !!!!\n");
    }

//    uint16_t temperature =0;
//    temperature = ((trans_desc.rx_data[0]&0b01111111)<<5) + ((trans_desc.rx_data[1]&0b11111000)>>3);
//    printf("max data_1: %x\n",temperature);
    uint16_t data=(trans_desc.rx_data[0]<<8 | trans_desc.rx_data[1]);

    return data;

    out:
        printf("error in spi init....\n");
    	return err;
}


float read_max_6675_temp(){
	float temperature=-999;
	max6675k_data_t* data;
	uint16_t max=read_max_6675_data();
	data=((max6675k_data_t*) &max);
	if(!data->thermocouple){
	    temperature=((float)data->temp * 0.25);
	 }
	return temperature;
}

