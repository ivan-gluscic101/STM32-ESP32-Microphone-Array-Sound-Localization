/**
  ******************************************************************************
  * @file    adc_capture.c
  * @brief   Continuous 4-channel ADC acquisition using circular DMA.
  ******************************************************************************
  */
#include "adc_capture.h"
#include "app_config.h"
#include "main.h"
#include <string.h>

/* ADC DMA buffer: two complete payload frames, no sync words here.
 * Half-transfer interrupt means dma_frame[0] is ready.
 * Transfer-complete interrupt means dma_frame[1] is ready.
 */
static uint16_t          dma_frame[2U * ADC_FRAME_LEN];

/* Two transmit buffers with sync header + copied payload.  The copy protects
 * the UART DMA from the ADC DMA overwriting the next circular half-buffer.
 */
static uint16_t          tx_frame[2U][ADC_TX_LEN];
static uint8_t           tx_index = 0U;
static bool              tx_prepared = false;

static volatile uint32_t ready_mask = 0U;     /* bit0: first half, bit1: second half */
static volatile uint32_t dropped_frames = 0U; /* increments when MATLAB/UART cannot keep up */
static bool              started = false;

/* --------------------------------------------------------------------------
 * Low-level peripheral init (GPIO analog inputs, TIM3 time base, ADC1 + DMA)
 * -------------------------------------------------------------------------- */
static void adc_gpio_init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

  /** ADC1 GPIO Configuration
   *  PB14 ------> ADC1_IN5   rank1, MATLAB MIC1
   *  PC0  ------> ADC1_IN6   rank2, MATLAB MIC2
   *  PC1  ------> ADC1_IN7   rank3, MATLAB MIC3
   *  PC2  ------> ADC1_IN8   rank4, MATLAB MIC4
   */
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;

  GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = LL_GPIO_PIN_1;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_14;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void tim3_init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct = {0};

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);

  TIM_InitStruct.Prescaler     = 0;
  TIM_InitStruct.CounterMode   = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload    = TIM3_AUTORELOAD;
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  LL_TIM_Init(TIM3, &TIM_InitStruct);
  LL_TIM_DisableARRPreload(TIM3);
  LL_TIM_SetClockSource(TIM3, LL_TIM_CLOCKSOURCE_INTERNAL);
  LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_UPDATE);
  LL_TIM_DisableMasterSlaveMode(TIM3);
}

static void adc_dma_init(void)
{
  /* DMA1_Channel1: ADC1 -> memory, half-words, circular continuous mode. */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_ADC1);
  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_VERYHIGH);
  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);
  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_HALFWORD);
  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_HALFWORD);
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_1,
                          LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA));
}

static void adc1_init(void)
{
  LL_ADC_InitTypeDef        ADC_InitStruct       = {0};
  LL_ADC_REG_InitTypeDef    ADC_REG_InitStruct   = {0};
  LL_ADC_CommonInitTypeDef  ADC_CommonInitStruct = {0};
  RCC_PeriphCLKInitTypeDef  PeriphClkInit        = {0};

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Adc12ClockSelection  = RCC_ADC12CLKSOURCE_SYSCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC12);

  ADC_InitStruct.Resolution    = LL_ADC_RESOLUTION_12B;
  ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
  ADC_InitStruct.LowPowerMode  = LL_ADC_LP_MODE_NONE;
  LL_ADC_Init(ADC1, &ADC_InitStruct);

  ADC_REG_InitStruct.TriggerSource    = LL_ADC_REG_TRIG_EXT_TIM3_TRGO;
  ADC_REG_InitStruct.SequencerLength  = LL_ADC_REG_SEQ_SCAN_ENABLE_4RANKS;
  ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
  ADC_REG_InitStruct.ContinuousMode   = LL_ADC_REG_CONV_SINGLE;
  ADC_REG_InitStruct.DMATransfer      = LL_ADC_REG_DMA_TRANSFER_UNLIMITED;
  ADC_REG_InitStruct.Overrun          = LL_ADC_REG_OVR_DATA_OVERWRITTEN;
  LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);

  LL_ADC_SetGainCompensation(ADC1, 0);
  LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);

  ADC_CommonInitStruct.CommonClock = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
  ADC_CommonInitStruct.Multimode   = LL_ADC_MULTI_INDEPENDENT;
  LL_ADC_CommonInit(__LL_ADC_COMMON_INSTANCE(ADC1), &ADC_CommonInitStruct);

  LL_ADC_REG_SetTriggerEdge(ADC1, LL_ADC_REG_TRIG_EXT_RISING);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_5);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_5, LL_ADC_SAMPLINGTIME_2CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_5, LL_ADC_SINGLE_ENDED);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, LL_ADC_CHANNEL_6);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_6, LL_ADC_SAMPLINGTIME_2CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_6, LL_ADC_SINGLE_ENDED);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, LL_ADC_CHANNEL_7);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_7, LL_ADC_SAMPLINGTIME_2CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_7, LL_ADC_SINGLE_ENDED);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, LL_ADC_CHANNEL_8);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_8, LL_ADC_SAMPLINGTIME_2CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_8, LL_ADC_SINGLE_ENDED);
}

static void wait_cpu_cycles(uint32_t cycles)
{
  while (cycles != 0U)
  {
    cycles--;
    __NOP();
  }
}

static void adc_enable_and_calibrate(void)
{
  LL_ADC_DisableDeepPowerDown(ADC1);
  LL_ADC_EnableInternalRegulator(ADC1);
  wait_cpu_cycles((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US * (SystemCoreClock / 1000000U)) / 2U);

  LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);
  while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0U)
  {
  }
  wait_cpu_cycles(LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES * 8U);

  LL_ADC_Enable(ADC1);
  while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0U)
  {
  }
}

static void init_tx_headers(void)
{
  for (uint32_t b = 0U; b < 2U; b++)
  {
    for (uint32_t i = 0U; i < FRAME_SYNC_LEN; i++)
    {
      tx_frame[b][i] = FRAME_SYNC_U16;
    }
  }
}

static void mark_half_ready(uint32_t half)
{
  const uint32_t bit = (1UL << half);

  if ((ready_mask & bit) != 0U)
  {
    dropped_frames++;
  }
  ready_mask |= bit;
}

static int32_t take_ready_half(void)
{
  int32_t half = -1;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if ((ready_mask & 0x01U) != 0U)
  {
    ready_mask &= ~0x01U;
    half = 0;
  }
  else if ((ready_mask & 0x02U) != 0U)
  {
    ready_mask &= ~0x02U;
    half = 1;
  }

  if (primask == 0U)
  {
    __enable_irq();
  }
  return half;
}

void AdcCapture_Init(void)
{
  init_tx_headers();
  adc_gpio_init();
  tim3_init();
  adc_dma_init();
  adc1_init();
  adc_enable_and_calibrate();
}

void AdcCapture_Start(void)
{
  if (started)
  {
    return;
  }

  ready_mask = 0U;
  dropped_frames = 0U;
  tx_prepared = false;

  LL_TIM_DisableCounter(TIM3);
  LL_TIM_SetCounter(TIM3, 0U);

  LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)dma_frame);
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, 2U * ADC_FRAME_LEN);
  LL_DMA_ClearFlag_HT1(DMA1);
  LL_DMA_ClearFlag_TC1(DMA1);
  LL_DMA_ClearFlag_TE1(DMA1);
  LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_1);
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);
  LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1);
  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

  LL_ADC_REG_StartConversion(ADC1);
  LL_TIM_EnableCounter(TIM3);
  started = true;
}

bool AdcCapture_FrameReady(void)
{
  if (tx_prepared)
  {
    return true;
  }

  int32_t half = take_ready_half();
  if (half < 0)
  {
    return false;
  }

  tx_index ^= 1U;
  for (uint32_t i = 0U; i < FRAME_SYNC_LEN; i++)
  {
    tx_frame[tx_index][i] = FRAME_SYNC_U16;
  }

  memcpy(&tx_frame[tx_index][FRAME_SYNC_LEN],
         &dma_frame[((uint32_t)half) * ADC_FRAME_LEN],
         ADC_FRAME_LEN * sizeof(uint16_t));

  tx_prepared = true;
  return true;
}

const uint16_t *AdcCapture_Buffer(void)
{
  tx_prepared = false;
  return tx_frame[tx_index];
}

uint32_t AdcCapture_DroppedFrames(void)
{
  return dropped_frames;
}

void AdcCapture_DmaIrqHandler(void)
{
  if (LL_DMA_IsActiveFlag_HT1(DMA1))
  {
    LL_DMA_ClearFlag_HT1(DMA1);
    mark_half_ready(0U);
  }

  if (LL_DMA_IsActiveFlag_TC1(DMA1))
  {
    LL_DMA_ClearFlag_TC1(DMA1);
    mark_half_ready(1U);
  }

  if (LL_DMA_IsActiveFlag_TE1(DMA1))
  {
    LL_DMA_ClearFlag_TE1(DMA1);
    dropped_frames++;
  }
}
