/*
 * si7021.c
 *
 *  Created on: 15.08.2018 ã.
 *      Author: imihalev
 */

#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "stdint.h"
#include "string.h"
#include "si7021.h"


#define I2CSDA 21
#define I2CSCL 22
#define SI7021_ADDR   0x40



#define	MRHHMM  0xE5   // Measure Relative Humidity, Hold Master Mode
#define MRHNHMM 0xF5   // Measure Relative Humidity, No Hold Master Mode
#define MTHMM   0xE3   // Measure Temperature, Hold Master Mode
#define MTNHMM  0xF3   // Measure Temperature, No Hold Master Mode
#define RTVPRH  0xE0   // Read Temperature Value from Previous RH Measurement
#define RESET   0xFE   // Reset
#define WUR1 0xE6   // Write RH/T User Register 1
#define RUR1 0xE7   // Read RH/T User Register 1
#define WCR    0x51   // Write Heater Control Register
#define RCR    0x11   // Read Heater Control Register
#define REID1_1 0xFA   // Read Electronic ID 1st Byte_1
#define REID1_2 0x0F   // Read Electronic ID 1st Byte_2
#define REID2_1 0xFC   // Read Electronic ID 2nd Byte_1
#define REID2_2 0xC9   // Read Electronic ID 2nd Byte_2
#define RFR_1   0x84   // Read Firmware Revision_1
#define RFR_2   0xB8   // Read Firmware Revision_2

struct si7021{


};


//#define i2c_port_t I2C_NUM_0

static i2c_port_t _port=I2C_NUM_0;
// i2c_port --> I2C_NUM_0

static bool i2c_init;


 static esp_err_t i2c_master_init(i2c_port_t i2c_port){
	printf("Init I2C....\n");
	esp_err_t err=ESP_OK;
	i2c_config_t conf;
	conf.mode=I2C_MODE_MASTER;
	conf.sda_io_num = I2CSDA;
	conf.scl_io_num = I2CSCL;
	conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	conf.master.clk_speed = 100000;
	err=i2c_param_config(i2c_port, &conf);
	err=i2c_driver_install(i2c_port, conf.mode,0,0,0);
	i2c_init=true;
	if(err!=ESP_OK)	printf("There are some err:%d\n",err);
	return err;
}

static bool check_crc(uint16_t data,uint8_t crc){

	uint32_t raw=(uint32_t)data <<8;
	//CRC generator polynomial of x^8 + x^5 + x^4 + 1 from 0x00--> 100110001
	uint32_t divisor = 0b100110001<<15;
    raw |= crc;

      for(int i=0 ; i<16;i++){
    	   if( raw &(uint32_t)1<<(23-i)){ //check last righ bit in raw
              raw ^=divisor;
    	   }
             divisor>>=1;
      }
         return(raw==0);

}

static esp_err_t readCommand(uint8_t command,uint8_t* data){
	 esp_err_t err=ESP_OK;
	 i2c_port_t i2c_port=_port;
	 if(!i2c_init ){
	 		 printf("I2C not found\n");
	 	  err=i2c_master_init(i2c_port);
	 	    if(err!=ESP_OK) goto END;
  	 }
	 i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	 ESP_ERROR_CHECK(i2c_master_start(cmd));
	 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,(SI7021_ADDR<<1) | I2C_MASTER_WRITE,1));
	 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,command,1));
	 ESP_ERROR_CHECK(i2c_master_start(cmd));
	 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,(SI7021_ADDR<<1) | I2C_MASTER_READ,1));
	 ESP_ERROR_CHECK(i2c_master_read_byte(cmd, data,1));
	 ESP_ERROR_CHECK(i2c_master_stop(cmd));
	 err = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
	 i2c_cmd_link_delete(cmd);
	 if(err!=ESP_OK)goto END;
	 END:
		if(err!=ESP_OK){
			printf("SI7021 Read Command -->I2C-->err:%d\n",err);
		}
		return err;
}

static esp_err_t writeCommand(uint8_t command,uint8_t* data){
	 esp_err_t err=ESP_OK;
	 i2c_port_t i2c_port=_port;
		 if(!i2c_init ){
		 		 printf("I2C not found\n");
		 	  err=i2c_master_init(i2c_port);
		 	    if(err!=ESP_OK) goto END;
	  	 }
		 i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		 ESP_ERROR_CHECK(i2c_master_start(cmd));
		 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,(SI7021_ADDR<<1) | I2C_MASTER_WRITE,1));
		 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,command,1));
		 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,*data,1));
		 ESP_ERROR_CHECK(i2c_master_stop(cmd));
		 err = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
		 i2c_cmd_link_delete(cmd);
		 if(err!=ESP_OK)goto END;
		 END:
			if(err!=ESP_OK){
				printf("SI7021 Read Command -->I2C-->err:%d\n",err);
			}
			return err;
}

esp_err_t  readTemperature(double *temperature){
	 i2c_port_t i2c_port=_port;
	 esp_err_t err=ESP_OK;
	 if(!i2c_init ){
		 printf("I2C not init\n");
	  err=i2c_master_init(i2c_port);
	    if(err!=ESP_OK) goto END;
	 }
	 uint8_t msb,lsb,crc;
	 i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	 ESP_ERROR_CHECK(i2c_master_start(cmd));
	 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,(SI7021_ADDR<<1) | I2C_MASTER_WRITE,1));
	 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,MTNHMM,1));
	 ESP_ERROR_CHECK(i2c_master_stop(cmd));
	 err = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
	 i2c_cmd_link_delete(cmd);
	 if(err!=ESP_OK)goto END;

	 vTaskDelay(20/portTICK_RATE_MS); //Waiting sensor
	  static uint8_t countRead=6;
      READ:
	 cmd = i2c_cmd_link_create();
	 ESP_ERROR_CHECK(i2c_master_start(cmd));
	 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,(SI7021_ADDR<<1) | I2C_MASTER_READ,1)); //Need wait for ack
	 ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &msb,0));
	 ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &lsb,0));
	 ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &crc,1));
	 ESP_ERROR_CHECK(i2c_master_stop(cmd));
	 err = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
	  //printf("Delete i2c cmd\n");
	 i2c_cmd_link_delete(cmd);
     if(err==ESP_OK){
	 //printf("msb:0x%x:\n",msb);
	 //printf("lsb:0x%x:\n",lsb);
	 //printf("crc:0x%x:\n",crc);

	 uint16_t raw_value=(uint16_t)msb<<8|(uint16_t)lsb;
	  if(check_crc(raw_value,crc)){
		  *temperature=((raw_value*175.72/65536.0)-46.85);
		  printf("Temperature is:%6.2f\n",*temperature);

	  } else {
		 //printf("CRC ERROR....!!!\n");
		 //printf("READ DATA one time....!!!\n");
		 countRead--;
		 if(countRead<=0) goto END;
		 goto READ;
	  }
     }
    END:
	if(err!=ESP_OK){
		printf("SI7021-->I2C-->err:%d\n",err);
	}
	return err;

 }

esp_err_t  readHumidity(double *humidity){
	 i2c_port_t i2c_port=_port;
	 esp_err_t err=ESP_OK;
	 if(!i2c_init ){
		 printf("I2C not init\n");
	  err=i2c_master_init(i2c_port);
	    if(err!=ESP_OK) goto END;
	 }
	 uint8_t msb,lsb,crc;
	 i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	 ESP_ERROR_CHECK(i2c_master_start(cmd));
	 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,(SI7021_ADDR<<1) | I2C_MASTER_WRITE,1));
	 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,MRHNHMM,1));
	 ESP_ERROR_CHECK(i2c_master_stop(cmd));
	 err = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
	 i2c_cmd_link_delete(cmd);
	 if(err!=ESP_OK)goto END;

	 vTaskDelay(20/portTICK_RATE_MS); //Waiting sensor
	  static uint8_t countRead=6;
      READ:
	 cmd = i2c_cmd_link_create();
	 ESP_ERROR_CHECK(i2c_master_start(cmd));
	 ESP_ERROR_CHECK(i2c_master_write_byte(cmd,(SI7021_ADDR<<1) | I2C_MASTER_READ,1)); //Need wait for ack
	 ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &msb,0));
	 ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &lsb,0));
	 ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &crc,1));
	 ESP_ERROR_CHECK(i2c_master_stop(cmd));
	 err = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
	  //printf("Delete i2c cmd\n");
	 i2c_cmd_link_delete(cmd);
     if(err==ESP_OK){
	 //printf("msb:0x%x:\n",msb);
	 //printf("lsb:0x%x:\n",lsb);
	 //printf("crc:0x%x:\n",crc);

	 uint16_t raw_value=(uint16_t)msb<<8|(uint16_t)lsb;
	  if(check_crc(raw_value,crc)){
		  *humidity=((raw_value*125/65536.0)-6.0);
		  printf("Humidity is:%6.2f\n",*humidity);

	  } else {
		 //printf("CRC ERROR....!!!\n");
		 //printf("READ DATA one time....!!!\n");
		 countRead--;
		 if(countRead<=0) goto END;
		 goto READ;
	  }
     }
    END:
	if(err!=ESP_OK){
		printf("SI7021-->I2C-->err:%d\n",err);
	}
	return err;

 }



esp_err_t readUserRegister(user_register_t* data){
	esp_err_t err=ESP_OK;
	uint8_t command=RUR1;
	err=readCommand(command,(uint8_t*)data);
   return err;
}

esp_err_t writeUserRegister(user_register_t* data){
	esp_err_t err=ESP_OK;
	uint8_t command=WUR1;
	err=writeCommand(command,(uint8_t*)data);
    return err;
}




