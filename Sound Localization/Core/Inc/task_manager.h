#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "queue.h"

/* Queue handles — definirani u task_manager.c, koriste stm32g4xx_it.c i taskovi */
extern osMessageQId  queueDmaEventHandle;  /* DMA ISR  → ACQ_Task  */
extern QueueHandle_t queueSnapshotHandle;  /* ACQ_Task → LOC_Task  */
extern QueueHandle_t queueResultHandle;    /* LOC_Task → UART_Task */

/**
 * Kreira queue-ove i FreeRTOS taskove (ACQ, FFT, UART).
 * Pozvati prije osKernelStart().
 */
void app_tasks_init(void);

#endif /* TASK_MANAGER_H */
