/**
 ******************************************************************************
 * @file    task_manager.c
 * @brief   Centralizirano kreiranje FreeRTOS taskova za pipeline 3D lokalizacije.
 *
 *  defaultTask (Idle)     — osDelay loop
 *  ACQ_Task    (Realtime) — DMA event → memcpy half-buffer → semAcqReady
 *  FFT_Task    (High)     — LOC3D_Process() → queueResultHandle
 *  UART_Task   (Low)      — queueResultHandle → UART_SendAngle3DPacket()
 *  GCC_Task    (Normal)   — rezervirano (npr. EMA filtriranje smjera)
 ******************************************************************************
 */

#include "task_manager.h"

/* StartXxx implementacije žive u main.c — koristimo extern deklaracije
 * umjesto dodatnog headera. Potpis je `void (void const *)` kako traži CMSIS-RTOS v1. */
extern void StartDefaultTask(void const *argument);
extern void StartACQTask    (void const *argument);
extern void StartFFTTask    (void const *argument);
extern void StartUARTTask   (void const *argument);
extern void StartTask05     (void const *argument);

void app_tasks_init(void)
{
    osThreadDef(defaultTask, StartDefaultTask, osPriorityIdle,     0, 128);
    (void)osThreadCreate(osThread(defaultTask), NULL);

    osThreadDef(ACQ_Task,    StartACQTask,     osPriorityRealtime, 0, 256);
    (void)osThreadCreate(osThread(ACQ_Task),   NULL);

    osThreadDef(FFT_Task,    StartFFTTask,     osPriorityHigh,     0, 512);
    (void)osThreadCreate(osThread(FFT_Task),   NULL);

    osThreadDef(UART_Task,   StartUARTTask,    osPriorityLow,      0, 256);
    (void)osThreadCreate(osThread(UART_Task),  NULL);

    osThreadDef(GCC_Task,    StartTask05,      osPriorityNormal,   0, 512);
    (void)osThreadCreate(osThread(GCC_Task),   NULL);
}
