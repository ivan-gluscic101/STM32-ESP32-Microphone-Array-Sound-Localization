/**
  ******************************************************************************
  * @file    adc_capture.c
  * @brief   Single-shot 4-channel ADC acquisition (LL drivers).
  ******************************************************************************
  */
#include "adc_capture.h"
#include "app_config.h"
#include "main.h"

/* TX buffer: [sync 0xFFFF 0xFFFF][interleaved CH1 CH2 CH3 CH4 x SAMPLES].
 * The ADC DMA writes only into the payload region (after the sync words);
 * the sync header is written once and never touched by the DMA. */
static uint16_t          adc_tx[ADC_TX_LEN];
static uint16_t * const  adc_frame = &adc_tx[FRAME_SYNC_LEN];
static volatile bool     frame_ready = false;

/* --------------------------------------------------------------------------
 * Low-level peripheral init (GPIO analog inputs, TIM3 time base, ADC1 + DMA)
 * -------------------------------------------------------------------------- */
static void adc_gpio_init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

  /**ADC1 GPIO Configuration
  PC0  ------> ADC1_IN6   (rank2, MATLAB CH2)
  PC1  ------> ADC1_IN7   (rank3, MATLAB CH3)
  PC2  ------> ADC1_IN8   (rank4, MATLAB CH4)
  PB14 ------> ADC1_IN5   (rank1, MATLAB CH1)
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
  TIM_InitStruct.Autoreload    = TIM3_AUTORELOAD;          /* 64 kHz update */
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  LL_TIM_Init(TIM3, &TIM_InitStruct);
  LL_TIM_DisableARRPreload(TIM3);
  LL_TIM_SetClockSource(TIM3, LL_TIM_CLOCKSOURCE_INTERNAL);
  LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_UPDATE);       /* TRGO on update -> ADC */
  LL_TIM_DisableMasterSlaveMode(TIM3);
}

static void adc_dma_init(void)
{
  /* DMA1_Channel1: ADC1 -> memory, half-words, normal (single frame) mode. */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_ADC1);
  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_VERYHIGH);
  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_NORMAL);
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

  /* ADC kernel clock = SYSCLK (170 MHz), divided by 4 below -> 42.5 MHz. */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Adc12ClockSelection  = RCC_ADC12CLKSOURCE_SYSCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC12);

  /* Core ADC config */
  ADC_InitStruct.Resolution    = LL_ADC_RESOLUTION_12B;
  ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
  ADC_InitStruct.LowPowerMode  = LL_ADC_LP_MODE_NONE;
  LL_ADC_Init(ADC1, &ADC_InitStruct);

  ADC_REG_InitStruct.TriggerSource    = LL_ADC_REG_TRIG_EXT_TIM3_TRGO;
  ADC_REG_InitStruct.SequencerLength  = LL_ADC_REG_SEQ_SCAN_ENABLE_4RANKS;
  ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
  ADC_REG_InitStruct.ContinuousMode   = LL_ADC_REG_CONV_SINGLE;
  ADC_REG_InitStruct.DMATransfer      = LL_ADC_REG_DMA_TRANSFER_LIMITED;
  ADC_REG_InitStruct.Overrun          = LL_ADC_REG_OVR_DATA_PRESERVED;
  LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);

  LL_ADC_SetGainCompensation(ADC1, 0);
  LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);

  ADC_CommonInitStruct.CommonClock = LL_ADC_CLOCK_SYNC_PCLK_DIV4;   /* 42.5 MHz */
  ADC_CommonInitStruct.Multimode   = LL_ADC_MULTI_INDEPENDENT;
  LL_ADC_CommonInit(__LL_ADC_COMMON_INSTANCE(ADC1), &ADC_CommonInitStruct);

  LL_ADC_REG_SetTriggerEdge(ADC1, LL_ADC_REG_TRIG_EXT_RISING);

  /* Sequencer: 2.5-cycle sampling on every rank (matches DelayProcessing.m). */
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

/* Cheap busy-wait helper (CPU cycles) used during the ADC power-up sequence. */
static void wait_cpu_cycles(uint32_t cycles)
{
  while (cycles != 0U)
  {
    cycles--;
    __NOP();
  }
}

/* Exit deep-power-down, start the internal regulator, run a single-ended
 * calibration, then enable the ADC. Order is mandatory on STM32G4. */
static void adc_enable_and_calibrate(void)
{
  LL_ADC_DisableDeepPowerDown(ADC1);
  LL_ADC_EnableInternalRegulator(ADC1);

  /* Wait for the ADC voltage regulator to stabilise (t_ADCVREG_STUP). */
  wait_cpu_cycles((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US * (SystemCoreClock / 1000000U)) / 2U);

  /* Calibrate in single-ended mode and wait for completion. */
  LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);
  while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0U)
  {
  }
  /* Mandatory delay between end of calibration and ADC enable.
   * The constant is in ADC clock cycles; the CPU runs ~4x faster than the
   * 42.5 MHz ADC clock, so scale up generously to stay on the safe side. */
  wait_cpu_cycles(LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES * 8U);

  LL_ADC_Enable(ADC1);
  while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0U)
  {
  }
}

void AdcCapture_Init(void)
{
  /* Write the sync header once; the ADC DMA never overwrites it. */
  adc_tx[0] = FRAME_SYNC_U16;
  adc_tx[1] = FRAME_SYNC_U16;

  adc_gpio_init();
  tim3_init();
  adc_dma_init();
  adc1_init();
  adc_enable_and_calibrate();
}

void AdcCapture_Start(void)
{
  frame_ready = false;

  /* (Re)arm DMA for one full frame. */
  LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)adc_frame);
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, ADC_FRAME_LEN);
  LL_DMA_ClearFlag_TC1(DMA1);
  LL_DMA_ClearFlag_TE1(DMA1);
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);
  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

  /* Allow the ADC to react to the external trigger, then release TIM3. */
  LL_ADC_REG_StartConversion(ADC1);
  LL_TIM_EnableCounter(TIM3);
}

bool AdcCapture_FrameReady(void)
{
  return frame_ready;
}

const uint16_t *AdcCapture_Buffer(void)
{
  /* Whole TX buffer including the leading sync header. */
  return adc_tx;
}

void AdcCapture_DmaIrqHandler(void)
{
  if (LL_DMA_IsActiveFlag_TC1(DMA1))
  {
    LL_DMA_ClearFlag_TC1(DMA1);

    /* Frame complete: stop triggering so the buffer stays stable. */
    LL_TIM_DisableCounter(TIM3);
    LL_ADC_REG_StopConversion(ADC1);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);

    frame_ready = true;
  }

  if (LL_DMA_IsActiveFlag_TE1(DMA1))
  {
    LL_DMA_ClearFlag_TE1(DMA1);
  }
}
