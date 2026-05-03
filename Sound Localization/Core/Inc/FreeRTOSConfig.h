/* USER CODE BEGIN Header */
/*
 * FreeRTOS Kernel V10.3.1
 * FreeRTOSConfig.h — ISPRAVLJENO
 *
 * ISPRAVKE:
 *
 *   1. configENABLE_FPU: 0 → 1
 *      STM32G474 ima hardverski FPU (Cortex-M4F).
 *      processTask koristi float (asinf, množenja, dijeljenja u LOC_Process).
 *      Bez ove postavke FreeRTOS NE čuva FPU registre pri context switchu.
 *      Ako scheduler preemptira processTask usred float operacije
 *      i drugi task dotakne float, FPU registri se pokvare.
 *      S configENABLE_FPU=1, lazy stacking čuva FPU kontekst samo kad treba.
 *
 *   2. configTOTAL_HEAP_SIZE: 12288 → 20480 (20 KB)
 *      Novi raspored:
 *        - processTask stack: 512 words × 4 =  2048 B
 *        - uartTask stack:    256 words × 4 =  1024 B
 *        - defaultTask stack: 128 words × 4 =   512 B
 *        - 2 queue-a + overhead:               ~600 B
 *        - Ukupno RTOS:                       ~4.2 KB
 *        - Slobodno za buduće taskove:        ~16 KB
 *      STM32G474RE ima 128 KB RAM-a, 20 KB heap je samo 15.6%.
 *
 *   3. configUSE_TIME_SLICING eksplicitno postavljeno na 1
 *      Originalno nije bilo definirano (koristio se default=1).
 *      Eksplicitnost je bolja od implicitnog oslanjanja na default.
 */
/* USER CODE END Header */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
  #include <stdint.h>
  extern uint32_t SystemCoreClock;
#endif

/* ── ISPRAVKA #1: FPU uključen ─────────────────────────────────────────── */
#define configENABLE_FPU                         1
#define configENABLE_MPU                         0

#define configUSE_PREEMPTION                     1
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCPU_CLOCK_HZ                       ( SystemCoreClock )
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     ( 7 )
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)
/* ── ISPRAVKA #2: Heap povećan ─────────────────────────────────────────── */
#define configTOTAL_HEAP_SIZE                    ((size_t)20480)
#define configMAX_TASK_NAME_LEN                  ( 16 )
#define configUSE_16_BIT_TICKS                   0
#define configUSE_MUTEXES                        1
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_PORT_OPTIMISED_TASK_SELECTION   1
/* ── ISPRAVKA #3: Time slicing eksplicitan ─────────────────────────────── */
#define configUSE_TIME_SLICING                   1

#define configMESSAGE_BUFFER_LENGTH_TYPE         size_t

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES                    0
#define configMAX_CO_ROUTINE_PRIORITIES          ( 2 )

#define configUSE_NEWLIB_REENTRANT               1

/* API functions */
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskCleanUpResources            0
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelayUntil                  0
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1

/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
 #define configPRIO_BITS         __NVIC_PRIO_BITS
#else
 #define configPRIO_BITS         4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY          ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* USER CODE BEGIN 1 */
#define configASSERT( x ) if ((x) == 0) {taskDISABLE_INTERRUPTS(); for( ;; );}
/* USER CODE END 1 */

#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

/* USER CODE BEGIN Defines */
/* USER CODE END Defines */

#endif /* FREERTOS_CONFIG_H */