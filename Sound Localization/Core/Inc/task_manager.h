#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "cmsis_os.h"

/**
 * @brief Kreira aplikacijske FreeRTOS taskove (ACQ, FFT, UART).
 *
 * Mora se pozvati NAKON što su queue-i (queueDmaEventHandle, queueSnapshotHandle,
 * queueResultHandle) već stvoreni u main(), a PRIJE osKernelStart().
 */
void app_tasks_init(void);

#endif /* TASK_MANAGER_H */
