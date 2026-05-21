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
  *    │  memcpy half-buffer → acq_snapshot[write_idx]  (dvostruki bafer)
  *    │  xQueueOverwrite → queueSnapshotHandle (uint8_t indeks)
  *    │  write_idx ^= 1  (naizmjenično 0↔1)
  *    ▼
  *  FFT_Task [High]
  *    │  xQueueReceive → read_idx
  *    │  shift+append → sliding_buf (1024 frejma = 16 ms zvuka)
  *    │  LOC3D_Process × 3 prozora (offset 0/256/512) → GCC + 3D smjer (~4.5 ms)
  *    │  xQueueSend → queueResultHandle (loc3d_result_t po vrijednosti)
  *    ▼
  *  UART_Task [Low]
  *    │  UART_SendAngle3DPacket → 10 B @ 921600 baud ≈ 0.1 ms
  *    ▼
  *  (čeka sljedeći event)
  *
  * ── Timing budget ───────────────────────────────────────────────────────────
  *  Half-buffer period = 512 uzoraka / 64 kHz = 8 ms
  *  Obrada: memcpy (~22 µs) + 3 × LOC3D_Process (~4.5 ms) + UART (~0.3 ms) ≈ 5 ms
  *  Margina: 8 ms − 5 ms = 3 ms → bez gubitka podataka, uz overhead i preempcije
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
#include <stdio.h>
/* USER CODE END Includes */

/* ── FreeRTOS handles ─────────────────────────────────────────────────────── */
/* USER CODE BEGIN PV */
osMessageQId  queueDmaEventHandle;  /* DMA ISR  → ACQ_Task  (uint32_t flag)   */
QueueHandle_t queueSnapshotHandle;  /* ACQ_Task → FFT_Task  (uint8_t indeks)  */
QueueHandle_t queueResultHandle;    /* FFT_Task → UART_Task (loc3d_result_t)  */

/* Dvostruki buffer — ACQ_Task upisuje indeks 0 ili 1 naizmjenično,
 * FFT_Task čita iz buffera čiji je indeks dobio kroz queue.
 * Dok FFT_Task čita buffer[i], ACQ_Task upisuje u buffer[1-i] → nema race-a.
 * 2 × 2048 × 2 B = 8 KB u BSS-u. */
static uint16_t acq_snapshot[2][HALF_BUFFER];

/* Sliding window buffer — drži [prethodni half | trenutni half] = 1024 frejma
 * = 16 ms zvuka. FFT_Task ga sklapa svakim novim DMA eventom i pokreće
 * lokalizaciju na 3 prozora po 512 frejmova (offset 0, 256, 512). Time je
 * pljesak koji padne na granicu dva DMA bloka uhvaćen u srednjem prozoru.
 *
 * Vlasništvo: pripada isključivo FFT_Task-u → nema concurrency problema.
 * 2 × 2048 × 2 B = 8 KB u BSS-u. */
static uint16_t sliding_buf[2 * HALF_BUFFER];
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
  /* Semafor više nije potreban — sinkronizacija ide kroz queueSnapshotHandle. */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_QUEUES */
  osMessageQDef(queueDmaEvent, 8, uint32_t);
  queueDmaEventHandle = osMessageCreate(osMessageQ(queueDmaEvent), NULL);

  /* Snapshot queue: ACQ_Task šalje indeks buffera (0 ili 1) po kopiranju.
   * Dubina 1 — ako FFT_Task kasni, stari indeks se odbacuje i šalje novi
   * kako bismo uvijek obrađivali najsvježije podatke. */
  queueSnapshotHandle = xQueueCreate(1, sizeof(uint8_t));

  /* Result queue: prosljeđuje cijeli loc3d_result_t po vrijednosti.
   * FreeRTOS queue obavlja memcpy → nema race-a između FFT i UART taska. */
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
 * ACQ_Task — čeka DMA event, kopira half-buffer u jedan od dva snapshot buffera,
 * zatim šalje indeks tog buffera FFT_Task-u kroz queue.
 *
 * Naizmjenično koristi indeks 0 i 1: dok FFT_Task obrađuje buffer[i],
 * ACQ_Task upisuje u buffer[1-i] — nema race conditiona.
 *
 * Ako FFT_Task kasni, xQueueOverwrite zamjenjuje stari indeks novim
 * kako bi FFT uvijek radio s najsvježijim podacima (preskačemo okvire
 * umjesto čekanja i rada s ustajalim podacima).
 */
void StartACQTask(void const *argument)
{
  uint8_t write_idx = 0;

  for (;;) {
    osEvent evt = osMessageGet(queueDmaEventHandle, osWaitForever);
    if (evt.status == osEventMessage) {
      uint32_t flag = evt.value.v;
      /* flag=0: HT interrupt → prva polovica [0 .. HALF_BUFFER-1]
       * flag=1: TC interrupt → druga polovica [HALF_BUFFER .. FULL_BUFFER-1] */
      const uint16_t *src = (flag == 0u)
                            ? &adc_buffer[0]
                            : &adc_buffer[HALF_BUFFER];
      memcpy(acq_snapshot[write_idx], src, HALF_BUFFER * sizeof(uint16_t));

      /* xQueueOverwrite: ne blokira i nikad ne puni queue — uvijek šalje
       * najnoviji indeks čak i ako FFT_Task još nije uzeo prethodni. */
      xQueueOverwrite(queueSnapshotHandle, &write_idx);

      write_idx ^= 1u;   /* izmjenično 0 → 1 → 0 → … */
    }
  }
}

/**
 * FFT_Task — sliding-window GCC + 3D lokalizacija.
 *
 * Strategija: pljesak ili kratki transient može pasti na granicu dva uzastopna
 * DMA half-buffera. Ako analiziramo samo 512 svježih frejmova, transient
 * razdvojen 50/50 daje slabu energiju u oba bloka pa silence-gate može propustiti
 * detekciju. Rješenje: drži posljednja 1024 frejma u `sliding_buf` i analiziraj
 * 3 prozora po 512 frejmova s offsetom 0 / 256 / 512.
 *
 *   sliding_buf layout (po DMA eventu):
 *     [ prethodnih 512 frejmova | novih 512 frejmova ]
 *     ▲              ▲              ▲
 *     offset 0       offset 256     offset 512
 *
 * Svaki uzorak je tako analiziran u barem 2 prozora — pljesak ne može
 * "promaknuti" između njih.
 *
 * Prvi DMA event nakon starta: prethodna polovica je nule → samo prozor
 * offset=512 daje smislen rezultat (silence-gate odbije ostala dva).
 */
void StartFFTTask(void const *argument)
{
  loc3d_result_t result;
  uint8_t read_idx;

  for (;;) {
    if (xQueueReceive(queueSnapshotHandle, &read_idx, portMAX_DELAY) == pdTRUE) {
      /* Shift: stara "druga polovica" postaje nova "prva polovica" */
      memcpy(&sliding_buf[0],
             &sliding_buf[HALF_BUFFER],
             HALF_BUFFER * sizeof(uint16_t));
      /* Append: najsvježiji half u drugu polovicu */
      memcpy(&sliding_buf[HALF_BUFFER],
             acq_snapshot[read_idx],
             HALF_BUFFER * sizeof(uint16_t));

      /* Tri prozora s 50% preklapanjem (offsetovi u frejmovima): */
      static const uint32_t offsets[3] = {
          0u,
          SAMPLES_PER_CHANNEL / 2u,
          SAMPLES_PER_CHANNEL
      };
      for (int i = 0; i < 3; i++) {
        if (LOC3D_Process(sliding_buf, offsets[i], &result)) {
          xQueueSend(queueResultHandle, &result, 0);
        }
      }
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
 * GCC_Task — periodično šalje FreeRTOS runtime stats kroz UART4.
 * Čeka 3 s pri pokretanju da taskovi nakupe mjerljivo CPU vrijeme,
 * zatim ispisuje snapshot svakih 10 s.
 */
void StartTask05(void const *argument)
{
  static TaskStatus_t tasks[10];
  static char line[64];
  osDelay(3000);
  for (;;) {
    /* NE koristimo pulTotalRunTime iz uxTaskGetSystemState — to je trenutni    */
    /* snapshot DWT/170 brojača koji wrappa svakih ~25 s. Umjesto toga,         */
    /* sumiramo ulRunTimeCounter svih taskova → suma = ukupno CPU vrijeme od   */
    /* boota (akumulirano kroz context switcheve, ispravno do wrapa za ~71 min)*/
    UBaseType_t n = uxTaskGetSystemState(tasks, 10, NULL);

    uint64_t total = 0;
    for (UBaseType_t i = 0; i < n; i++) {
      total += tasks[i].ulRunTimeCounter;
    }
    if (total == 0) {
      osDelay(10000);
      continue;
    }

    UART_SendString("\r\nTask             CPU (us)   CPU%\r\n");
    UART_SendString("-----------------------------------\r\n");
    for (UBaseType_t i = 0; i < n; i++) {
      uint32_t pct = (uint32_t)(((uint64_t)tasks[i].ulRunTimeCounter * 100ULL) / total);
      snprintf(line, sizeof(line), "%-16s %9lu   %3lu%%\r\n",
               tasks[i].pcTaskName,
               tasks[i].ulRunTimeCounter,
               pct);
      UART_SendString(line);
    }
    UART_SendString("-----------------------------------\r\n");
    osDelay(10000);
  }
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
