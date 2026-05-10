#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "cmsis_os.h"
#include <math.h>

/**
 * @brief Inicijalizira FreeRTOS taskove za testiranje
 *        (Mock_Test_Task simulira zvučni izvor)
 */
void app_tasks_init(void);

#endif /* TASK_MANAGER_H */
