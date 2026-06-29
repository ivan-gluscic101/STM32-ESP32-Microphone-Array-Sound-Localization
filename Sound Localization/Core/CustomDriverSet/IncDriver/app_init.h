#ifndef APP_INIT_H
#define APP_INIT_H

/*
 * app_init.h — Aplikacijska inicijalizacija (sve izvan CubeMX periferija).
 *
 * Drži main.c čistim: I2C + IMU init, dijagnostički ispis i kalibracija žira
 * žive ovdje, ne u USER CODE blokovima main.c-a. Periferije (ADC/DMA/UART/TIM)
 * i dalje inicijalizira CubeMX (MX_*), a RTOS taskovi/queue-ovi su u
 * task_manager.c (app_tasks_init).
 */

/*
 * Inicijalizira I2C1 (LL) i MPU IMU, ispiše dijagnostiku na UART te kalibrira
 * žiro ako je čip prisutan. Pozvati iz main() prije app_tasks_init().
 * Ploča mora biti NEPOMIČNA tijekom poziva (kalibracija žira).
 */
void app_init(void);

#endif /* APP_INIT_H */
