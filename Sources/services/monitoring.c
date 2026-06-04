/*
 * monitoring.c
 *
 *  Created on: May 22, 2026
 *      Author: JunsangS
 */


#include "../automotive.h"

typedef struct {
    uint32_t canId;
    uint8_t nodeId;
    TickType_t lastRxTick;
    uint16_t timeoutMs;
    bool timeoutError;
} ComRxMonitor_t;


