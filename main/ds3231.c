/*
 * ds3223.c
 *
 *  Created on: 8.06.2018 ã.
 *      Author: imihalev
 */


#include "ds3231.h"

static const char *TAGDS="DS3231: ";
static i2c_port_t i2c_port=I2C_NUM_0;

static uint8_t  hex2bcd(uint8_t data)
{
   uint8_t data_second_decimal=data/10;
   uint8_t data_first_decimal=data - 10*data_second_decimal;
   data=16*data_second_decimal + data_first_decimal;
  return data;
}

static uint8_t  bcd2hex(uint8_t bcd)
{
	uint8_t hex=0;
	hex=(bcd>>4)*10 + (bcd&0x0f);
	return hex;
}

static void displayBuffer(uint8_t* buffer, int len){
	int i;
	for(i= 0;i < len;i++){
		printf("%02x ",buffer[i]);
		if((i+1)%16==0){
			printf("\n");
		}
	}
	printf("\n");
}

esp_err_t SetTimeDate(uint8_t* data_set ) {
	int ret;
	uint8_t* data = malloc(8);
	displayBuffer(data_set,7);
	if (data_set[0] > 59)
		data[0] = 0;
	else
		data[0] = hex2bcd(data_set[0]);
	if (data_set[1] > 59)
		data[1] = 0;
	else
		data[1] = hex2bcd(data_set[1]);
	if (data_set[2] > 23)
		data[2] = 0;
	else
		data[2] = hex2bcd(data_set[2]);
	if (data_set[3] > 7)
		data[3] = 1;
	else
		data[3] = hex2bcd(data_set[3]);
	if (data_set[4] == 0 || data_set[4] > 31)
		data[4] = 1;
	else
		data[4] = hex2bcd(data_set[4]);
	if (data_set[5] == 0 || data_set[5] > 12)
		data[5] = 1;
	else
		data[5] = hex2bcd(data_set[5]);
	if (data_set[6] > 99)
		data[6] = 0;
	else
		data[6] = hex2bcd(data_set[6]);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (DS3231_ADDRESS << 1) | I2C_MASTER_WRITE, 1);
	i2c_master_write_byte(cmd, 0, 1);
	i2c_master_write(cmd, data, 7,1);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
	vTaskDelay(10 / portTICK_PERIOD_MS); //Wait from  write cycle in device
	if (ret != 0) {
		ESP_LOGD(TAGDS, "ERROR WRITE TIME DATE --> err: %d",ret);
		return ret;
	}
	return ret;
	return true;
}

time_t read_ds3231_time() {
	//Check for I2C
	int ret;
	struct tm tm;
	uint8_t data[7];
	uint8_t len=7;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	ESP_ERROR_CHECK(i2c_master_start(cmd));
	ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (DS3231_ADDRESS << 1) | I2C_MASTER_WRITE, 1));
	ESP_ERROR_CHECK(i2c_master_write_byte(cmd, 0, 1));
	ESP_ERROR_CHECK(i2c_master_start(cmd));
	ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (DS3231_ADDRESS << 1) | I2C_MASTER_READ, 1));
	ESP_ERROR_CHECK(i2c_master_read(cmd, data, len-1,0)); //read all whit ack.
	ESP_ERROR_CHECK(i2c_master_read(cmd, data+(len-1), 1,1)); //read last whit nack.
	ESP_ERROR_CHECK(i2c_master_stop(cmd));
	ret=i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
	 if(ret!=ESP_OK){
	  ESP_LOGD(TAGDS, "ERROR READ TIME DS3231 -->ret: %d", ret);
	  i2c_cmd_link_delete(cmd);
	  return -1;
	 }
	i2c_cmd_link_delete(cmd);
//	if (ret != 0) {
//		ESP_LOGD(TAGDS, "ERROR READ DS3231 -->ret: %d", ret);
//		return ret;
//	}
//
	tm.tm_sec=bcd2hex(data[0]);
	tm.tm_min=bcd2hex(data[1]);
	tm.tm_hour=bcd2hex(data[2]);
	tm.tm_mday=bcd2hex(data[4]);
	tm.tm_mon=bcd2hex(data[5])-1;
	tm.tm_year=bcd2hex(data[6])+100;
	time_t readTime=mktime(&tm);
	printf("DS1307 Read Time is:%lu\n",readTime);
	return readTime;
}

esp_err_t ReadDS1307_Temperature(char* temp) {
	int ret;
	uint8_t* msb=malloc(1);
	uint8_t* lsb=malloc(1);
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (DS3231_ADDRESS << 1) | I2C_MASTER_WRITE, 1);
	i2c_master_write_byte(cmd, TEMPERATURE_ADDRESS, 1);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (DS3231_ADDRESS << 1) | I2C_MASTER_READ, 1);
	i2c_master_read_byte(cmd, msb, 0); //read all whit ack.
	i2c_master_read_byte(cmd, lsb, 1); //read last whit nack.
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
	if (ret != 0) {
		temp="";
		ESP_LOGD(TAGDS, "ERROR READ TEMEPATURE DS3231 -->ret: %d", ret);
		return ret;
	}
	uint8_t t1 = 0;
	uint8_t t2 = 0;
	char s = '+';
	if (msb[0] & 0x80)
		s = '-';
	switch (lsb[0] >> 6) {
	case 1:
		t2 = 25;
		break;
	case 2:
		t2 = 50;
		break;
	case 3:
		t2 = 75;
		break;
	default:
		t2 = 0;
	}
	t1 = msb[0] & 0x7f;
	sprintf(temp, "%c%02d.%02d", s, t1, t2);
	//ESP_LOGD(TAGDS, "Temperature: %s", temp);
	free(msb);
	free(lsb);
	return ret;
}
