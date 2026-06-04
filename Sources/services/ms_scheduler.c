/*
 * ms_scheduler.c
 *
 *  Created on: May 21, 2026
 *      Author: JunsangS
 */

#include "../automotive.h"

volatile uint32_t gComScheduler10msCount = 0U;
volatile uint32_t gComScheduler100msCount = 0U;
volatile uint32_t gComScheduler1000msCount = 0U;
volatile uint32_t gComSchedulerLastCycle10ms = 0U;

void COM_Task10ms(void)
{
    gComScheduler10msCount++;
}

void COM_Task100ms(void)
{
    gComScheduler100msCount++;
}

void COM_Task1000ms(void)
{
    gComScheduler1000msCount++;
}

void vComSchedulerTask(void *pvParameters)
{
    TickType_t lastWakeTime;
    uint32_t cycle10ms = 0U;

    (void)pvParameters;

    lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        COM_Task10ms();

        cycle10ms++;
        gComSchedulerLastCycle10ms = cycle10ms;

        if ((cycle10ms % 10U) == 0U)
        {
            COM_Task100ms();
        }

        if ((cycle10ms % 100U) == 0U)
        {
            COM_Task1000ms();
            cycle10ms = 0U;
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10U));
    }
}
