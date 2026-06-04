/*
 * system_init.c
 *
 *  Created on: May 20, 2026
 *      Author: JunsangS
 */

#include "../automotive.h"

#define CAN_INIT_LED_PORT    PTC
#define CAN_INIT_LED_PIN     4U

void system_init() {
	/* Write your local variable definition here */
	/*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
	#ifdef PEX_RTOS_INIT
		PEX_RTOS_INIT();   /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
	#endif
	CLOCK_SYS_Init(g_clockManConfigsArr, // CLOCK INIT
			CLOCK_MANAGER_CONFIG_CNT,
			g_clockManCallbacksArr,
			CLOCK_MANAGER_CALLBACK_CNT);
	CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_AGREEMENT); //CLOCK UPDATE
	PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr); // PIN INIT
	UART_Init(&uart_pal1_instance, &uart_pal1_Config0); // UART INIT
}

void	can_init() {
    status_t can_status = CAN_Init(&can_pal1_instance, &can_pal1_Config0);
    if (can_status == STATUS_SUCCESS) {
        PINS_DRV_ClearPins(CAN_INIT_LED_PORT, (pins_channel_type_t)(1UL << CAN_INIT_LED_PIN));
    }
}
