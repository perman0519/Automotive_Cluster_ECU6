/*
 * gpio.c
 *
 *  Created on: May 21, 2026
 *      Author: JunsangS
 */


#include "../automotive.h"

/* This example is setup to work by default with DEVKIT. To use it with other boards
   please comment the following line
*/

#define DEVKIT

#ifdef DEVKIT
	#define LED1_PORT	PTA
    #define LED1 		10          /* pin PA[10] - LED1 (DS4) on DEV-KIT */
	#define LED2_PORT	PTJ
    #define LED2 		4	       /* pin PJ[4] - LED2 (DS9) on DEV-KIT */
	#define LED3_PORT	PTH
    #define LED3 		5          /* pin PH[5] - LED3 (DS8) on DEV-KIT */
	#define LED4_PORT	PTC
    #define LED4 		4          /* pin PC[4] - LED4 (DS7) on DEV-KIT */
#endif
uint32_t leds[] = {LED1, LED2, LED3, LED4};
GPIO_Type * ports[] = {LED1_PORT, LED2_PORT, LED3_PORT, LED4_PORT};


void vLEDTask( void *pvParameters )
{
    unsigned int ID = (unsigned int)pvParameters;
    for( ;; )
    {
        /* Not very exciting - just delay... */
        vTaskDelay( mainLED_DELAY/(ID+1) );
        PINS_DRV_ClearPins(ports[ID], (1 << leds[ID]));
        //LOG_Print("LOG", "INFO", "HEARTBEAT LED TURN OFF");
        /* delay */
        vTaskDelay( mainLED_DELAY/((3-ID)+1) );
        PINS_DRV_SetPins(ports[ID], (1 << leds[ID]));
        //LOG_Print("LOG", "INFO", "HEARTBEAT LED TURN ON");
	}
}
