/*
 * automotive.h
 *
 *  Created on: May 20, 2026
 *      Author: JunsangS
 */

#ifndef AUTOMOTIVE_H_
#define AUTOMOTIVE_H_

#include "Cpu.h"
#include "FreeRTOS.h"
#include "clockMan1.h"
#include "can_pal1.h"
#include "pin_mux.h"
#include "task.h"
#include "dmaController1.h"
#include "pin_mux.h"
#include "uart_pal1.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


//GPIO
#define mainLED_DELAY                        ( ( TickType_t ) 500 / portTICK_PERIOD_MS )
#define mainLED_TASK_PRIORITY                ( tskIDLE_PRIORITY + 2 )
void vLEDTask( void *pvParameters );

// CAN
#ifndef CAN_NODE_ID
#define CAN_NODE_ID                          1U
#endif

#define CAN_NODE_COUNT                       6U
#define CAN_TX_PERIOD_MS                     100U
#define CAN_TX_TASK_PRIORITY                 ( tskIDLE_PRIORITY + 3 )
void vCANTask( void *pvParameters );
status_t CAN_SendMessage(const can_message_t *msg, uint32_t timeoutMs);
status_t CAN_SendOtaResponse(const can_message_t *msg, uint32_t timeoutMs);
status_t CAN_ReceiveMessage(can_message_t *msg, uint32_t timeoutMs);

// UART
#define TIMEOUT         200UL
#define BUFFER_SIZE     256UL

void rxCallback(void *driverState, uart_event_t event, void *userData);

// logging
#define ECU_NUM 6U
#define LOG_TIMEOUT_MS   200U
#define LOG_BUFFER_SIZE  256U
#define RTOS_RTOS_INIT "RTOS_INIT"
void vInitLogTask(void *pvParameters);

typedef struct s_uart_logger {
	//time
	int8_t ecu_num; //ECUx
	int8_t module;
	int8_t level;
} t_uart_logger;

// clock
#define LOG_UART_INSTANCE uart_pal1_instance
#define LOG_TIMEOUT       100U

// services
void	system_init();
void	can_init();
void	LOG_Print(const char *module,
               const char *level,
               const char *message);
void	COM_GatewayInit(void);
void	COM_GatewayHandleRxMessage(const can_message_t *msg);
void	COM_GatewayCheckTimeouts(void);
void	COM_GatewayBuildStatusMessage(can_message_t *msg);
bool	OTA_HandleRequestMessage(const can_message_t *request,
	                            can_message_t *response);
void	vComSchedulerTask(void *pvParameters);

#endif /* AUTOMOTIVE_H_ */
