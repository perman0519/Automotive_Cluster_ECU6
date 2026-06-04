/*
 * uart.c
 *
 *  Created on: May 21, 2026
 *      Author: JunsangS
 */


#include "../automotive.h"

uint8_t buffer[BUFFER_SIZE];
uint8_t bufferIdx;

void rxCallback(void *driverState, uart_event_t event, void *userData)
{
    /* Unused parameters */
    (void)driverState;
    (void)userData;
    /* Check the event type */
    if (event == UART_EVENT_RX_FULL)
    {
        /* The reception stops when newline is received or the buffer is full */
        if ((buffer[bufferIdx] != '\n') && (bufferIdx != (BUFFER_SIZE - 2U)))
        {
            /* Update the buffer index and the rx buffer */
            bufferIdx++;
            UART_SetRxBuffer(&uart_pal1_instance, &buffer[bufferIdx], 1U);
        }
    }
}
