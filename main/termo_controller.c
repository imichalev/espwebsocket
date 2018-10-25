

#include "termo_controller.h"
#include "MAX6675K.h"
#include "esp_system.h"
#include "soc/spi_periph.h"
#include "esp_err.h"
#include "time.h"
#include "MAX6675K.h"


uint8_t temp_settup=45;
uint8_t temp_error=0;
uint8_t temp_read;

struct{
 uint8_t initPower :1;


}controller;

static esp_err_t init_power(){
 controller.initPower=0;
 esp_err_t err=ESP_OK;
 err=gpio_set_direction(POWER_PIN,GPIO_MODE_OUTPUT);
 if(err!=ESP_OK) goto END;
 err=gpio_set_level(POWER_PIN,POWER_OFF);
 if(err!=ESP_OK) goto END;
 if(gpio_get_level(POWER_PIN)!=POWER_OFF){
	 printf("Error in POWER GPIO....\n");
	 err= ESP_FAIL;
 }
 END:
  return err;
}


esp_err_t controllerOn (){   //Call from main thread every 2 second;
	static float temp_err=0.0;
	static float temp_settup=80.0;

	//time_t time_used;
	time_t now;
	//time(&now);
	//time(&time_on);
	//time(&time_off);
	static time_t time_on;
	static time_t time_off;
	static time_t deltatime_on=0;
	static time_t deltatime_off=0;
	static bool check=0;

	esp_err_t err=ESP_OK;
   if(!controller.initPower){
	//try to init power pin
	  if(init_power()==ESP_OK){
		controller.initPower=1;
	  }else{
		  err=ESP_FAIL;
		  goto END;
       }
   }

   float currentTemperature=read_max_6675_temp();
    printf ("From controllerON temp is:%6.2f\n",currentTemperature);
   if(currentTemperature!=-999){
	   //Check for error
	   printf("From controller time_on:%d\n",(uint32_t)deltatime_on);
	   printf("From controller time_off:%d\n",(uint32_t)deltatime_off);
	   temp_err=currentTemperature-temp_settup;
	   if(temp_err>0){
		   gpio_set_level(POWER_PIN,POWER_OFF);
		   //only one time
		   if(check){
           time(&now);
           time_off=now;
           deltatime_on=now-time_on;
           check=0;
		   }
           //time_on=now;
           //printf("From controller time_off:%d\n",(uint32_t)time_off);
	   }else{
		   gpio_set_level(POWER_PIN,POWER_ON);
		   //only time
		   if(!check){
		   time(&now);
		   time_on=now;
		   deltatime_off=now-time_off;
		   check=1;
		   }
		   //time_off=now;
		   //printf("From controller time_on:%d\n",(uint32_t)time_on);

	   }
   }else{
	    gpio_set_level(POWER_PIN,POWER_OFF);
   }



 END:
  return err;
}
