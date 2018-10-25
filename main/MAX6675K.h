/*
 * MAX6675K.h
 *
 *  Created on: 12.07.2018 ã.
 *      Author: imihalev
 */

#ifndef MAIN_MAX6675K_H_
#define MAIN_MAX6675K_H_


#define PIN_NUM_CS_MAX    3
#define PIN_NUM_CLK_MAX  16
#define PIN_NUM_MISO_MAX  17

typedef struct{
	uint16_t state:1;
	uint16_t id:1;
	uint16_t thermocouple:1;
	uint16_t temp:12;
	uint16_t sign:1;
}max6675k_data_t;

uint16_t read_max_6675_data();
float read_max_6675_temp();

#endif /* MAIN_MAX6675K_H_ */
