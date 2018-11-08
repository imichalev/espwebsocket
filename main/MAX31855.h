/*
 * MAX6675K.h
 *
 *  Created on: 12.07.2018 ã.
 *      Author: imihalev
 */

#ifndef MAIN_MAX31855_H_
#define MAIN_MAX31855_H_


#include "tftspi.h"

#define PIN_NUM_CS_MAX    3
#define PIN_NUM_CLK_MAX   16
#define PIN_NUM_MISO_MAX  17

//typedef struct{
//	uint16_t tempouts:1;
//	uint16_t tempout:13;
//	uint16_t reserved0:1;
//	uint16_t fault:1;
//	uint16_t tempins:1;
//	uint16_t tempin:11;
//	uint16_t reserved1:1;
//	uint16_t scvfault:1;
//	uint16_t scgfault:1;
//	uint16_t ocfault:1;
//}max31855_data_t;


typedef struct{
	uint16_t ocfault:1;
	uint16_t scgfault:1;
	uint16_t scvfault:1;
	uint16_t reserved1:1;
	uint16_t tempin:11;
	uint16_t tempins:1;
	uint16_t fault:1;
	uint16_t reserved0:1;
	uint16_t tempout:13;
	uint16_t tempouts:1;
}max31855_data_t;


extern spi_lobo_device_handle_t max_spi;


//uint32_t read_max_data_lobo();
uint32_t read_max_data();
uint16_t read_max_tempin();
uint16_t read_max_tempout();

#endif /* MAIN_MAX31855_H_ */
