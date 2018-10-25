/*
 * ds3231.h
 *
 *  Created on: 8.06.2018 ã.
 *      Author: imihalev
 */

#ifndef MAIN_DS3231_H_
#define MAIN_DS3231_H_

#include "stdint.h"
#include "driver/i2c.h"
//#include "user_config.h"
#include "esp_log.h"
#include "time.h"

typedef struct
{
 uint8_t second; //0x00
 uint8_t minute;
 uint8_t hour;
 uint8_t day;
 uint8_t date;
 uint8_t month;
 uint8_t year;
 uint8_t a1m1;
 uint8_t a1m2;
 uint8_t a1m3;
 uint8_t alm4;
 uint8_t a2m2;
 uint8_t a2m3;
 uint8_t a2m4;
 uint8_t control;
 uint8_t controlStatus;
 uint8_t agingoffset;
 uint8_t msbtemp;
 uint8_t lsbtemp;
}ds3231_t;

#define DS3231_ADDRESS 0x68 //0xD0
#define TEMPERATURE_ADDRESS  0x11

time_t read_ds3231_time();
esp_err_t SetTimeDate(uint8_t* data_set);
esp_err_t ReadDS1307_Temperature(char* temp);

//bool ReadDS1307(void);
//bool ReadDS1307_Temperature(void);
//bool SetTimeDate(uint8_t second,uint8_t minute,uint8_t hours,uint8_t day,uint8_t date, uint8_t month,uint8_t year);

#endif /* MAIN_DS3231_H_ */
