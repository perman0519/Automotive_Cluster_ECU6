/* ###################################################################
**     Filename    : main.c
**     Project     : Automotive_Cluster
**     Processor   : MPC5748G_324
**     Version     : Driver 01.00
**     Compiler    : GNU C Compiler
**     Date/Time   : 2017-03-14, 14:08, # CodeGen: 1
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/

#include "automotive.h"
volatile int exit_code = 0;

/*! 
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
*/

int main(void)		{
  /* Write your local variable definition here */

	system_init();
	can_init();

	xTaskCreate(vInitLogTask,
		  ( const char * const )"InitLog",
		  configMINIMAL_STACK_SIZE * 2,
		  NULL,
		  mainLED_TASK_PRIORITY, NULL);
	xTaskCreate( vLEDTask,
		  ( const char * const )"LedTask",
		  configMINIMAL_STACK_SIZE * 2,
		  (void*)0,
		  mainLED_TASK_PRIORITY+1, NULL );
	xTaskCreate( vCANTask,
		  ( const char * const )"CanTask",
		  configMINIMAL_STACK_SIZE * 4,
		  NULL,
		  CAN_TX_TASK_PRIORITY, NULL );
	xTaskCreate( vComSchedulerTask,
		  ( const char * const )"SchedulerTask",
		  configMINIMAL_STACK_SIZE * 2,
		  NULL,
		  CAN_TX_TASK_PRIORITY, NULL );
	vTaskStartScheduler();

  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;) {
    if(exit_code != 0) {
      break;
    }
  }
  return exit_code;
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
}
