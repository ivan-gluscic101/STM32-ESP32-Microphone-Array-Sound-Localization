/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   Aplikacijska točka ulaza — high-level FreeRTOS pregled
  *
  * ── Pipeline ────────────────────────────────────────────────────────────────
  *
  *  DMA ISR (NVIC prio 5)
  *    │  xQueueSendFromISR → queueDmaEventHandle (flag: 0=prva pol., 1=druga)
  *    ▼
  *  ACQ_Task [Realtime]
  *    │  memcpy half-buffer → acq_snapshot[]
  *    │  osSemaphoreRelease → semAcqReady
  *    ▼
  *  FFT_Task [High]
  *    │  LOC3D_Process(acq_snapshot) → GCC + 3D smjer (~1.5 ms @ 170 MHz FPU)
  *    │  osMessagePut → queueResultHandle (pointer na statički result)
  *    ▼
  *  UART_Task [Low]
  *    │  UART_SendAngle3DPacket → 10 B @ 921600 baud ≈ 0.1 ms
  *    ▼
  *  (čeka sljedeći event)
  *
  * ── Timing budget ───────────────────────────────────────────────────────────
  *  Half-buffer period = 512 uzoraka / 64 kHz = 8 ms
  *  Obrada: memcpy (~12 µs) + GCC × 3 (~1.5 ms) + UART (~0.1 ms) ≈ 1.6 ms
  *  Margina: 8 ms − 1.6 ms = 6.4 ms → bez gubitka podataka
  *
  * ── Stack analiza ───────────────────────────────────────────────────────────
  *  defaultTask:  128 words (512 B) — osDelay loop, OK
  *  ACQ_Task:     256 words (1 KB)  — memcpy + semaphore, nema float, OK
  *  FFT_Task:     512 words (2 KB)  — float aritmetika + FPU kontekst (~128 B)
  *                                    svi veliki nizovi su static u sound_loc_3d.c
  *                                    procijenjeno ~500 B max → 2 KB je dostatno
  *  GCC_Task:     512 words (2 KB)  — rezervirano za buduće proširenje
  *  UART_Task:    256 words (1 KB)  — UART byte slanje, OK
  *  Ukupno stack: 6656 B + 5 × TCB (~140 B) + 3 queues + 1 sem ≈ 8 KB
  *  Heap limit (FreeRTOSConfig.h): 16 KB — dostatno
  *
  * ── Prioriteti ──────────────────────────────────────────────────────────────
  *  ACQ_Task (Realtime) preemptira sve → buffer kopiran odmah po DMA eventu
  *  FFT_Task (High) preemptira GCC i UART → obrada ima prednost pred slanjem
  *  UART_Task (Low) nikad ne blokira obradu
  *  DMA NVIC prio 5 = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY → ISR-safe
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include "audio_common.h"
#include "gpio_driver.h"
#include "dma_driver.h"
#include "adc_driver.h"
#include "timer_driver.h"
#include "uart_driver.h"
#include "sound_loc_3d.h"
#include "task_manager.h"
#include <string.h>
/* USER CODE END Includes */

/* ── FreeRTOS handles ─────────────────────────────────────────────────────── */
/* USER CODE BEGIN PV */
osMessageQId  queueDmaEventHandle;  /* DMA ISR  → ACQ_Task  (uint32_t flag)   */
QueueHandle_t queueResultHandle;    /* FFT_Task → UART_Task (loc3d_result_t)  */

osSemaphoreId semAcqReady;          /* ACQ_Task → FFT_Task                   */

/* Half-buffer kopija — ACQ_Task piše, FFT_Task čita.
 * 2048 × 2 B = 4 KB u BSS-u (ne na stacku). */
static uint16_t acq_snapshot[HALF_BUFFER];
/* USER CODE END PV */

/* ── Task prototipi (definicije ovdje, kreiranje u task_manager.c) ────────── */
void StartDefaultTask(void const *argument);
void StartACQTask(void const *argument);
void StartUARTTask(void const *argument);
void StartFFTTask(void const *argument);
void StartTask05(void const *argument);
void SystemClock_Config(void);

/* ─────────────────────────────────────────────────────────────────────────── */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */
  
  HAL_Init();
  SystemClock_Config();

  /* USER CODE BEGIN 2 */
  Custom_GPIO_Init();
  Custom_DMA_Init();
  Custom_ADC_Init();
  Custom_UART4_Init();
  Custom_TIM8_Init();
  Custom_ACQ_Start();   /* kalibrira ADC, postavlja DMA, pali TIM8 trigger */
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  osSemaphoreDef(semAcqReady);
  semAcqReady = osSemaphoreCreate(osSemaphore(semAcqReady), 1);
  osSemaphoreWait(semAcqReady, 0);   /* odmah preuzmi token → FFT_Task blokira */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_QUEUES */
  osMessageQDef(queueDmaEvent, 8, uint32_t);
  queueDmaEventHandle = osMessageCreate(osMessageQ(queueDmaEvent), NULL);

  /* Result queue: prosljeđuje cijeli loc3d_result_t (6 B) po vrijednosti.   *
   * FreeRTOS queue obavlja memcpy unutar xQueueSend/Receive → nema race-a   *
   * između FFT_Task i UART_Task, čak i pri preempciji.                      */
  queueResultHandle = xQueueCreate(4, sizeof(loc3d_result_t));
  /* USER CODE END RTOS_QUEUES */

  /* USER CODE BEGIN RTOS_THREADS */
  /* Kreiranje svih taskova centralizirano u task_manager.c. */
  app_tasks_init();
  /* USER CODE END RTOS_THREADS */

  BSP_LED_Init(LED_GREEN);
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  osKernelStart();
  while (1) {}
}

/* ── Task implementacije ──────────────────────────────────────────────────── */

void StartDefaultTask(void const *argument)
{
  for (;;) { osDelay(1); }
}

/**
 * ACQ_Task — čeka DMA event, kopira half-buffer, signalizira FFT_Task.
 * Prioritet Realtime osigurava da se kopija dogodi ODMAH — DMA ne smije
 * prepisati half-buffer dok ga kopiramo (kopija traje ~12 µs, margina ~8 ms).
 */
void StartACQTask(void const *argument)
{
  for (;;) {
    osEvent evt = osMessageGet(queueDmaEventHandle, osWaitForever);
    if (evt.status == osEventMessage) {
      uint32_t flag = evt.value.v;
      /* flag=0: HT interrupt → prva polovica [0 .. HALF_BUFFER-1]
       * flag=1: TC interrupt → druga polovica [HALF_BUFFER .. FULL_BUFFER-1] */
      const uint16_t *src = (flag == 0u)
                            ? &adc_buffer[0]
                            : &adc_buffer[HALF_BUFFER];
      memcpy(acq_snapshot, src, HALF_BUFFER * sizeof(uint16_t));
      osSemaphoreRelease(semAcqReady);
    }
  }
}

/**
 * FFT_Task — GCC + 3D lokalizacija.
 * xQueueSend kopira loc3d_result_t (6 B) u queue → result može biti lokalna varijabla.
 */
void StartFFTTask(void const *argument)
{
  loc3d_result_t result;

  for (;;) {
    osSemaphoreWait(semAcqReady, osWaitForever);

    if (LOC3D_Process(acq_snapshot, &result)) {
      xQueueSend(queueResultHandle, &result, 0);
    }
  }
}

/**
 * UART_Task — šalje 3D lokalizacijski paket kad FFT_Task detektira zvuk.
 * 10 bajta @ 921600 baud ≈ 108 µs.
 */
void StartUARTTask(void const *argument)
{
  loc3d_result_t r;

  for (;;) {
    if (xQueueReceive(queueResultHandle, &r, portMAX_DELAY) == pdTRUE) {
      UART_SendAngle3DPacket(r.az_tenth, r.el_tenth, r.strength);
    }
  }
}

/**
 * GCC_Task — rezervirano (npr. za EMA glađenje kuta ili budući 3D prikaz).
 */
void StartTask05(void const *argument)
{
  for (;;) { osDelay(10); }
}

/* ── Sistemski callback-i i pomoćne funkcije ─────────────────────────────── */

void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
  while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4);

  LL_PWR_EnableRange1BoostMode();
  LL_RCC_HSI_Enable();
  while (LL_RCC_HSI_IsReady() != 1);

  LL_RCC_HSI_SetCalibTrimming(64);
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_4, 85, LL_RCC_PLLR_DIV_2);
  LL_RCC_PLL_EnableDomain_SYS();
  LL_RCC_PLL_Enable();
  while (LL_RCC_PLL_IsReady() != 1);

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_2);
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);

  /* 1 µs prijelazno stanje pri intermediate clock-u */
  for (__IO uint32_t i = (170 >> 1); i != 0; i--);

  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_SetSystemCoreClock(170000000);

  if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK) { Error_Handler(); }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6) { HAL_IncTick(); }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
