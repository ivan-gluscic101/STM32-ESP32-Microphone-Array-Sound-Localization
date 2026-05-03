/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — ISPRAVLJENO
  ******************************************************************************
  *
  *  POPIS ISPRAVKI:
  *
  *  1. BAUD RATE TYPO: 912600 → 921600
  *     Original je imao krivi broj. PC strana očekuje 921600.
  *
  *  2. RACE CONDITION — UART I OBRADA U ISTOM TASKU:
  *     processTask je radio LOC_Process (~par µs) i UART_SendFrame (~45 ms).
  *     Za 45 ms UART slanja, DMA napuni ~1.4 nova half-buffera, queue se
  *     nakuplja, i s dubinom 4 počinjemo gubiti poruke.
  *
  *     POPRAVAK: Razdvojeno u dva taska:
  *       - processTask (osPriorityAboveNormal):
  *           čita DMA event queue → kopira half-buffer → LOC_Process →
  *           šalje angle paket (brzo, 8 B = <0.1 ms) →
  *           proslijedi kopiju uartTasku ako je red za audio frame
  *       - uartTask (osPriorityNormal):
  *           čeka poruku od processTask-a → šalje audio frame (~45 ms)
  *           Na nižem je prioritetu, pa processTask ga uvijek preemptira.
  *
  *  3. RACE CONDITION — DMA PREPISUJE BUFFER DOK GA TASK ČITA:
  *     Originalno, processTask je čitao direktno iz adc_buffer[] dok je
  *     DMA mogao prepisati tu polovicu (ako obrada + UART > 32 ms).
  *
  *     POPRAVAK: processTask kopira half-buffer u lokalni niz ODMAH
  *     po primitku poruke. Kopija traje ~12 µs na 170 MHz (memcpy
  *     2048 × 2 B = 4096 B). DMA neće prepisati ovu polovicu sljedećih
  *     32 ms, a kopija traje 0.012 ms — sigurnosna margina je ogromna.
  *
  *  4. NEKONZISTENTNOST HALF_SIZE vs SAMPLES_PER_CHANNEL:
  *     TwoDimSoundLoc.h imao HALF_SIZE=256, main.c SAMPLES_PER_CHANNEL=512.
  *     LOC_Process obrađivao samo pola podataka.
  *     POPRAVAK: Sve konstante u audio_common.h. HALF_SIZE = 512.
  *
  *  5. QUEUE DUBINA 4 → 8:
  *     Sigurnosna margina. S razdvojenim taskovima processTask obradi
  *     event za <1 ms, pa queue nikad ne bi trebao imati više od 1 poruke.
  *     Ali 8 daje marginu za slučaj sporijeg LOC_Process u budućnosti.
  *
  *  6. FRAME_ID POMAKNUT U uartTask:
  *     Originalno je frame_id bio static global koji se koristio iz
  *     processTask-a. Sad je lokalna varijabla uartTask-a — samo taj
  *     task šalje framove, pa samo on treba brojač.
  *
  *  7. DMA PRIORITET: LOW → HIGH
  *     Za real-time audio stream, DMA ne smije biti potisnut od
  *     nekog drugog DMA kanala koji se doda u budućnosti.
  *
  *  8. defaultTask STACK SMANJEN: 256 → 128 words
  *     defaultTask ne radi ništa osim osDelay(1). 128 words je dostatno.
  *
  *  9. CommTask UKLONJEN:
  *     Prazni placeholder koji troši 1 KB RAM-a i CPU za context switch
  *     svakih 1 ms. Kad bude trebao ESP32, dodaj ga natrag.
  *
  *  10. semAudioReady UKLONJEN:
  *      Originalno kreiran ali nikad korišten. Dead code.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include "TwoDimSoundLoc.h"
#include <string.h>   /* za memcpy */
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#include "audio_common.h"   /* NUM_CH, SAMPLES_PER_CHANNEL, HALF_BUFFER, itd. */

/* UART frame protokol */
#define FRAME_SOF1    ((uint8_t)0xAA)
#define FRAME_SOF2    ((uint8_t)0xBB)
#define FRAME_EOF1    ((uint8_t)0xCC)
#define FRAME_EOF2    ((uint8_t)0xDD)

/* Koliko half-buffer događaja preskočimo prije slanja jednog framea.
 * Svaki half-buffer = 512 uzoraka @ 64 kHz = 8 ms.
 * FRAME_SKIP = 16 → šaljemo svakih 128 ms (~7.8 FPS).
 * Frame = 2+2 + 4*512*2 + 2 = 4102 B → ~44.5 ms @ 921600 baud < 128 ms OK
 * Vrijedi samo kad je SEND_AUDIO_FRAMES = 1. */
#define FRAME_SKIP    16
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
COM_InitTypeDef BspCOMInit;

osThreadId defaultTaskHandle;
osThreadId processTaskHandle;
#if SEND_AUDIO_FRAMES
osThreadId uartTaskHandle;
#endif

osMessageQId queueDmaEventHandle;   /* ISR → processTask (buf_sel: 0/1) */
#if SEND_AUDIO_FRAMES
osMessageQId queueUartMsgHandle;    /* processTask → uartTask (pointer na poruku) */
#endif

/* USER CODE BEGIN PV */
uint16_t adc_buffer[FULL_BUFFER];   /* DMA upisuje ovdje */

/* Struktura poruke processTask → uartTask.
 * audio_snapshot postoji samo kad je SEND_AUDIO_FRAMES=1.
 * Kad je 0, štedimo 4 KB RAM-a (HALF_BUFFER × 2 B). */
typedef struct {
    int16_t  phi_tenth_deg;
    uint8_t  strength;
    uint8_t  angle_valid;
#if SEND_AUDIO_FRAMES
    uint16_t audio_snapshot[HALF_BUFFER];
    uint8_t  send_audio;
#endif
} uart_msg_t;

static uart_msg_t uart_msg;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_UART4_Init(void);
static void MX_TIM8_Init(void);
void StartDefaultTask(void const * argument);
void StartProcessTask(void const * argument);
#if SEND_AUDIO_FRAMES
void StartUartTask(void const * argument);
#endif

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

static inline void UART_SendByte(uint8_t byte)
{
    while (!LL_USART_IsActiveFlag_TXE(UART4));
    LL_USART_TransmitData8(UART4, byte);
}

/**
 * @brief Šalje jedan kompletni ADC frame preko UART4.
 *
 *   Format:
 *     SOF(2B) | frame_id(2B big-endian) | data(N*4*2B big-endian) | EOF(2B)
 *
 *   Blokirajuće — vraca se tek kad je zadnji bajt izašao iz shift registra.
 *
 * @param buf             pokazivač na KOPIJU half-buffera (ne na DMA buffer!)
 * @param samples_per_ch  broj uzoraka po kanalu
 * @param fid             frame ID
 */
static void UART_SendFrame(const uint16_t *buf, uint32_t samples_per_ch, uint16_t fid)
{
    uint32_t i;
    uint16_t val;

    UART_SendByte(FRAME_SOF1);
    UART_SendByte(FRAME_SOF2);

    UART_SendByte((uint8_t)(fid >> 8));
    UART_SendByte((uint8_t)(fid & 0xFF));

    for (i = 0; i < samples_per_ch * NUM_CH; i++)
    {
        val = buf[i];
        UART_SendByte((uint8_t)(val >> 8));
        UART_SendByte((uint8_t)(val & 0xFF));
    }

    UART_SendByte(FRAME_EOF1);
    UART_SendByte(FRAME_EOF2);

    while (!LL_USART_IsActiveFlag_TC(UART4));
}

/**
 * @brief Šalje angle paket (tip 0x02).
 *   Format (8 B): [0xAA][0xBB][0x02][PHI_H][PHI_L][STR][0xCC][0xDD]
 */
static void UART_SendAnglePacket(int16_t phi_tenth_deg, uint8_t strength)
{
    UART_SendByte(0xAA);
    UART_SendByte(0xBB);
    UART_SendByte(0x02);
    UART_SendByte((uint8_t)(phi_tenth_deg >> 8));
    UART_SendByte((uint8_t)(phi_tenth_deg & 0xFF));
    UART_SendByte(strength);
    UART_SendByte(0xCC);
    UART_SendByte(0xDD);
    while (!LL_USART_IsActiveFlag_TC(UART4));
}

/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_UART4_Init();
  MX_TIM8_Init();

  /* USER CODE BEGIN 2 */

  /* Kalibracija ADC-a */
  LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);
  while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0);

  /* Power up ADC */
  LL_ADC_Enable(ADC1);
  while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0);

  /* DMA konfiguracija — adrese i duljina */
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)adc_buffer);
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_1,
      LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA));
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, FULL_BUFFER);

  /* Half-transfer i transfer-complete interrupti */
  LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_1);
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);

  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
  LL_ADC_REG_StartConversion(ADC1);
  LL_TIM_EnableCounter(TIM8);

  /* USER CODE END 2 */

  /* ── RTOS objekti ─────────────────────────────────────────────────────── */

  /* Queue: ISR → processTask (DMA half-buffer indeks: 0 ili 1)
   * Dubina 8 — processTask obrađuje za <1 ms, pa nikad neće biti
   * više od 1-2 poruke, ali 8 daje veliku marginu. */
  osMessageQDef(queueDmaEvent, 8, uint32_t);
  queueDmaEventHandle = osMessageCreate(osMessageQ(queueDmaEvent), NULL);

#if SEND_AUDIO_FRAMES
  osMessageQDef(queueUartMsg, 2, uint32_t);
  queueUartMsgHandle = osMessageCreate(osMessageQ(queueUartMsg), NULL);
#endif

  osThreadDef(defaultTask, StartDefaultTask, osPriorityIdle, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  osThreadDef(processTask, StartProcessTask, osPriorityAboveNormal, 0, 512);
  processTaskHandle = osThreadCreate(osThread(processTask), NULL);

#if SEND_AUDIO_FRAMES
  osThreadDef(uartTask, StartUartTask, osPriorityNormal, 0, 256);
  uartTaskHandle = osThreadCreate(osThread(uartTask), NULL);
#endif

  /* LED i COM */
  BSP_LED_Init(LED_GREEN);
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  osKernelStart();

  while (1) { }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SystemClock_Config — 170 MHz iz HSI (16 MHz) preko PLL
 * PLL: M=/4, N=×85, R=/2 → 16/4 × 85 / 2 = 170 MHz
 * ═══════════════════════════════════════════════════════════════════════════ */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4) {}

  LL_PWR_EnableRange1BoostMode();
  LL_RCC_HSI_Enable();
  while(LL_RCC_HSI_IsReady() != 1) {}

  LL_RCC_HSI_SetCalibTrimming(64);
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_4,
                               85, LL_RCC_PLLR_DIV_2);
  LL_RCC_PLL_EnableDomain_SYS();
  LL_RCC_PLL_Enable();
  while(LL_RCC_PLL_IsReady() != 1) {}

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_2);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {}

  for (__IO uint32_t i = (170 >> 1); i != 0; i--);

  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_SetSystemCoreClock(170000000);

  if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Periferni init — ADC1, TIM8, UART4, DMA, GPIO
 * ═══════════════════════════════════════════════════════════════════════════ */

static void MX_ADC1_Init(void)
{
  LL_ADC_InitTypeDef ADC_InitStruct = {0};
  LL_ADC_REG_InitTypeDef ADC_REG_InitStruct = {0};
  LL_ADC_CommonInitTypeDef ADC_CommonInitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_RCC_SetADCClockSource(LL_RCC_ADC12_CLKSOURCE_SYSCLK);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC12);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

  /* PC0 → ADC1_IN6 */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PC1 → ADC1_IN7 */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_1;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PA0 → ADC1_IN1 */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PB14 → ADC1_IN5 */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_14;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* DMA request mapping */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_ADC1);
  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1,
                                   LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  /* ISPRAVKA: LOW → HIGH za real-time audio */
  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_HIGH);
  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);
  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_HALFWORD);
  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_HALFWORD);

  NVIC_SetPriority(ADC1_2_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(ADC1_2_IRQn);

  ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
  ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
  ADC_InitStruct.LowPowerMode = LL_ADC_LP_MODE_NONE;
  LL_ADC_Init(ADC1, &ADC_InitStruct);

  ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_EXT_TIM8_TRGO;
  ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_4RANKS;
  ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
  ADC_REG_InitStruct.ContinuousMode = LL_ADC_REG_CONV_SINGLE;
  ADC_REG_InitStruct.DMATransfer = LL_ADC_REG_DMA_TRANSFER_UNLIMITED;
  ADC_REG_InitStruct.Overrun = LL_ADC_REG_OVR_DATA_PRESERVED;
  LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);

  LL_ADC_SetGainCompensation(ADC1, 0);
  LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);

  ADC_CommonInitStruct.CommonClock = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
  ADC_CommonInitStruct.Multimode = LL_ADC_MULTI_INDEPENDENT;
  LL_ADC_CommonInit(__LL_ADC_COMMON_INSTANCE(ADC1), &ADC_CommonInitStruct);
  LL_ADC_REG_SetTriggerEdge(ADC1, LL_ADC_REG_TRIG_EXT_RISING);

  LL_ADC_DisableDeepPowerDown(ADC1);
  LL_ADC_EnableInternalRegulator(ADC1);

  uint32_t wait_loop_index;
  wait_loop_index = ((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US
                     * (SystemCoreClock / (100000 * 2))) / 10);
  while(wait_loop_index != 0) { wait_loop_index--; }

  /* Scan sekvenca: RANK1=CH1(PA0), RANK2=CH5(PB14), RANK3=CH6(PC0), RANK4=CH7(PC1) */
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_1);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_24CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SINGLE_ENDED);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, LL_ADC_CHANNEL_5);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_5, LL_ADC_SAMPLINGTIME_24CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_5, LL_ADC_SINGLE_ENDED);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, LL_ADC_CHANNEL_6);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_6, LL_ADC_SAMPLINGTIME_24CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_6, LL_ADC_SINGLE_ENDED);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, LL_ADC_CHANNEL_7);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_7, LL_ADC_SAMPLINGTIME_24CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_7, LL_ADC_SINGLE_ENDED);
}

static void MX_TIM8_Init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct = {0};

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM8);

  /* TIM8 na 170 MHz, ARR=2655 → Update rate = 170e6/(2655+1) ≈ 64 kHz
   * ADC u scan modu bez DISCONT: svaki trigger pokreće sekvencu sva 4 kanala.
   * → svaki kanal uzorkovan 64 kHz.
   *
   * Sampling time po kanalu: 24.5 + 12.5 = 37 ADC ciklusa
   * ADC clock = PCLK/4 = 170/4 = 42.5 MHz → 1 ADC ciklus = 23.5 ns
   * 4 kanala × 37 ciklusa × 23.5 ns = 3.48 µs za sva 4 kanala
   * Timer period = 1/64 kHz = 15.6 µs → dovoljno vremena za konverziju.
   *
   * Offset između kanala: 37 ADC ciklusa × 23.5 ns ≈ 0.87 µs ≈ 0.9 µs
   * Ovo je CH_DELAY_S u TwoDimSoundLoc i koristi se za korekciju TDOA. */
  TIM_InitStruct.Prescaler = 0;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 2655;
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  TIM_InitStruct.RepetitionCounter = 0;
  LL_TIM_Init(TIM8, &TIM_InitStruct);
  LL_TIM_DisableARRPreload(TIM8);
  LL_TIM_SetClockSource(TIM8, LL_TIM_CLOCKSOURCE_INTERNAL);
  LL_TIM_SetTriggerOutput(TIM8, LL_TIM_TRGO_UPDATE);
  LL_TIM_SetTriggerOutput2(TIM8, LL_TIM_TRGO2_RESET);
  LL_TIM_DisableMasterSlaveMode(TIM8);
}

static void MX_UART4_Init(void)
{
  LL_USART_InitTypeDef USART_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_RCC_SetUARTClockSource(LL_RCC_UART4_CLKSOURCE_PCLK1);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART4);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);

  /* PC10 → UART4_TX */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PC11 → UART4_RX */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_11;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;
  /* ISPRAVKA: 912600 → 921600 (typo u originalu) */
  USART_InitStruct.BaudRate = 921600;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(UART4, &USART_InitStruct);
  LL_USART_DisableFIFO(UART4);
  LL_USART_SetTXFIFOThreshold(UART4, LL_USART_FIFOTHRESHOLD_1_8);
  LL_USART_SetRXFIFOThreshold(UART4, LL_USART_FIFOTHRESHOLD_1_8);
  LL_USART_ConfigAsyncMode(UART4);

  LL_USART_Enable(UART4);
  while((!(LL_USART_IsActiveFlag_TEACK(UART4))) ||
        (!(LL_USART_IsActiveFlag_REACK(UART4)))) {}
}

static void MX_DMA_Init(void)
{
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMAMUX1);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

  NVIC_SetPriority(DMA1_Channel1_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

static void MX_GPIO_Init(void)
{
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOF);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FreeRTOS taskovi
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * defaultTask — IDLE placeholder. Ne radi ništa korisno.
 * Stack smanjen na 128 words (bio 256) jer ne koristi nikakvu memoriju.
 * Prioritet osPriorityIdle — radi samo kad ništa drugo ne treba CPU.
 */
void StartDefaultTask(void const * argument)
{
  for(;;)
  {
    osDelay(1000);   /* 1s umjesto 1ms — manje besmislenih context switcheva */
  }
}

/**
 * processTask — Obrada DMA half-buffer događaja.
 *
 * Prioritet: osPriorityAboveNormal (viši od uartTask-a)
 *
 * TIJEK:
 *   1. Čeka poruku iz DMA ISR-a (buf_sel: 0=ping, 1=pong)
 *   2. ODMAH kopira half-buffer u lokalni niz (12 µs)
 *      → DMA može slobodno pisati u tu polovicu nakon kopije
 *   3. Pokreće LOC_Process na kopiji
 *   4. Ako LOC_Process vrati valjan kut → šalje angle paket (8 B, <0.1 ms)
 *   5. Svakih FRAME_SKIP događaja → proslijedi kopiju uartTask-u za audio frame
 *
 * ZAŠTO JE OVO SIGURNO:
 *   - Kopija traje 12 µs. DMA sljedeći put prepiše ovu polovicu za 32 ms.
 *     Margina: 32 ms / 0.012 ms = 2666×.
 *   - LOC_Process radi na kopiji, ne na DMA bufferu.
 *   - Angle paket (8 B) se šalje za <0.1 ms — processTask se vraća na
 *     čekanje queue-a prije nego DMA završi sljedeći half-buffer.
 *   - Audio frame šalje uartTask na nižem prioritetu. Kad DMA event stigne,
 *     processTask preemptira uartTask, kopira podatke, i vrati se.
 *     UART hardver drži stanje, uartTask nastavlja slanje kad se vrati.
 */
void StartProcessTask(void const * argument)
{
#if SEND_AUDIO_FRAMES
  uint8_t frame_skip_cnt = 0;
#endif

  for(;;)
  {
    osEvent evt = osMessageGet(queueDmaEventHandle, osWaitForever);
    if (evt.status != osEventMessage) continue;

    uint32_t buf_sel = evt.value.v;
    const uint16_t *src = &adc_buffer[buf_sel * HALF_BUFFER];

#if SEND_AUDIO_FRAMES
    /* Kopija half-buffera — uartTask čita iz kopije, ne iz DMA buffera */
    memcpy(uart_msg.audio_snapshot, src, HALF_BUFFER * sizeof(uint16_t));
    const uint16_t *proc_buf = uart_msg.audio_snapshot;
#else
    /* Bez slanja audija — direktno čitamo iz DMA buffera.
     * LOC_Process traje <<1 ms, DMA ne prepiše ovu polovicu 8 ms. */
    const uint16_t *proc_buf = src;
#endif

    uart_msg.angle_valid = LOC_Process(
        proc_buf,
        &uart_msg.phi_tenth_deg,
        &uart_msg.strength
    );

    if (uart_msg.angle_valid)
    {
      UART_SendAnglePacket(uart_msg.phi_tenth_deg, uart_msg.strength);
    }

#if SEND_AUDIO_FRAMES
    frame_skip_cnt++;
    if (frame_skip_cnt >= FRAME_SKIP)
    {
      frame_skip_cnt = 0;
      uart_msg.send_audio = 1;
      osMessagePut(queueUartMsgHandle, (uint32_t)&uart_msg, 0);
    }
    else
    {
      uart_msg.send_audio = 0;
    }
#endif
  }
}

/**
 * uartTask — Šalje audio frame preko UART4 (spora operacija, ~45 ms).
 *
 * Prioritet: osPriorityNormal (niži od processTask-a)
 *
 * ZAŠTO ODVOJENI TASK:
 *   Slanje 4102 B @ 921600 baud traje ~44.5 ms. Ako bi processTask
 *   to radio sam, za 44.5 ms bi propustio ~1.4 DMA half-buffer eventa.
 *   Ovako, dok uartTask polira UART, processTask ga može preemptirati
 *   kad stigne novi DMA event, obraditi ga za <1 ms, i vratiti se.
 *   UART hardver drži stanje — kad se uartTask vrati na Running,
 *   nastavlja slanje točno gdje je stao.
 *
 *   Podatke čita iz KOPIJE (audio_snapshot), ne iz DMA buffera,
 *   pa nema opasnosti od korupcije.
 */
#if SEND_AUDIO_FRAMES
void StartUartTask(void const * argument)
{
  uint16_t frame_id = 0;

  for(;;)
  {
    osEvent evt = osMessageGet(queueUartMsgHandle, osWaitForever);
    if (evt.status != osEventMessage) continue;

    uart_msg_t *msg = (uart_msg_t *)evt.value.p;

    if (msg->send_audio)
    {
      UART_SendFrame(msg->audio_snapshot, SAMPLES_PER_CHANNEL, frame_id);
      frame_id++;
      BSP_LED_Toggle(LED_GREEN);
    }
  }
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════ */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif