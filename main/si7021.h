/*
 * si7021.h
 *
 *  Created on: 15.08.2018 ã.
 *      Author: imihalev
 */

#ifndef MAIN_SI7021_H_
#define MAIN_SI7021_H_

#include "esp_system.h"
#include "esp_err.h"
#include "driver/i2c.h"


typedef struct {
	uint8_t RES1:1;
	uint8_t VDSS:1;
	uint8_t RSVD0:3;
	uint8_t HTRE:1;
	uint8_t RSVD1:1;
	uint8_t RES0:1;
}user_register_t;


esp_err_t readTemperature(double *temperature);
esp_err_t readHumidity(double *humidity);
esp_err_t readUserRegister(user_register_t* data);
esp_err_t writeUserRegister(user_register_t* data);


#endif /* MAIN_SI7021_H_ */
