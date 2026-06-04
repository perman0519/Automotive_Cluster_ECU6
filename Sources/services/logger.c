/*
 * logger.c
 *
 *  Created on: May 20, 2026
 *      Author: JunsangS
 */

#include "../automotive.h"

void LOG_Print(const char *module,
               const char *level,
               const char *message)
{
    TickType_t tick = xTaskGetTickCount();
    uint32_t time_ms = tick * portTICK_PERIOD_MS;
    char buffer[LOG_BUFFER_SIZE];

    snprintf(buffer,
             sizeof(buffer),
             "[%06lu][ECU%u][%s][%s] %s\r\n",
             (unsigned long)time_ms,
			 ECU_NUM,
             module,
             level,
             message);

    UART_SendDataBlocking(&uart_pal1_instance,
                          (uint8_t *)buffer,
                          strlen(buffer),
                          LOG_TIMEOUT_MS);
}

void vInitLogTask(void *pvParameters)
{
    (void)pvParameters;
    LOG_Print("INIT", "INFO", "Boot completed"); //	    LOG_Print("INIT", "INFO", "PEX_RTOS_INIT completed");
	LOG_Print("INIT", "INFO", "CLOCK_INIT completed");
	LOG_Print("INIT", "INFO", "PINS_DRV_INIT completed");
	LOG_Print("INIT", "INFO", "UART_INIT completed");
	LOG_Print("INIT", "INFO", "CAN_INIT completed");
	LOG_Print("INIT", "INFO", "Boot completed");
	LOG_Print("INIT", "INFO", "RTOS vTaskStartScheduler starting...");
    vTaskDelete(NULL);
}
