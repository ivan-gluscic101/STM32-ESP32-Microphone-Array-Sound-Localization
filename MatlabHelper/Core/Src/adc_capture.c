/**
  ******************************************************************************
  * @file    adc_capture.c
  * @brief   Slobodno-tekući 4-kanalni ADC (circular DMA) s detekcijom pljeska
  *          na čipu. UART-u se predaje samo kratak prozor oko svakog
  *          detektiranog događaja, umjesto slanja svega.
  ******************************************************************************
  */
#include "adc_capture.h"
#include "app_config.h"
#include "main.h"
#include <string.h>

/* ADC DMA double buffer: dva bloka. HT prekid => blok 0 gotov,
 * TC prekid => blok 1 gotov. Jedan prekid po ADC_BLOCK_SAMPLES. */
static uint16_t dma_buf[2U * ADC_BLOCK_LEN];

/* Pred-okidni povijesni prsten sirovih interleaveanih uzoraka (HIST_BLOCKS blokova). */
static uint16_t hist[HIST_BLOCKS * ADC_BLOCK_LEN];

/* Sastavljeni paket događaja: sync zaglavlje + EVENT_WIN_BLOCKS kopiranih blokova. */
static uint16_t tx_packet[ADC_TX_LEN];

/* --- stanje detekcije (mijenja se u ISR-u, čita u glavnoj petlji) --- */
typedef enum
{
  ST_FILL = 0,     /* uči DC + šumni pod, prsten još nije napunjen     */
  ST_IDLE,         /* naoružan, čeka okidanje                          */
  ST_CAPTURE,      /* okidanje viđeno, čeka dolazak POST blokova       */
  ST_REFRACTORY    /* događaj uhvaćen, neko vrijeme ignoriraj re-okide */
} evstate_t;

static volatile evstate_t state       = ST_FILL;
static int32_t            dc[ADC_NUM_CHANNELS]; /* DC pratitelj po kanalu */
static uint64_t           nf_energy   = 0U;     /* prilagodljiva energija šumnog poda */
static uint32_t           blocks_seen = 0U;
static uint32_t           hist_wr     = 0U;     /* sljedeći blok prstena za upis */

static volatile uint32_t  trig_block  = 0U;     /* indeks bloka okidanja u prstenu */
static volatile uint32_t  post_left   = 0U;
static volatile uint32_t  refr_left   = 0U;

/* Predaja ISR -> glavna petlja. */
static volatile bool      assemble_req = false; /* ISR: prozor spreman za kopiranje */
static volatile uint32_t  win_start_block = 0U; /* najstariji blok prozora u prstenu */
static volatile bool      event_ready  = false; /* main: tx_packet popunjen, pošalji ga */
static volatile uint32_t  event_count  = 0U;
static volatile uint32_t  dropped_events = 0U;

static bool started = false;

/* --------------------------------------------------------------------------
 * Nisko-razinski init periferija (GPIO analog, TIM3 vrem. baza, ADC1+DMA)
 * -------------------------------------------------------------------------- */
static void adc_gpio_init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

  /** ADC1 GPIO konfiguracija
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
  /* DMA1_Channel1: ADC1 -> memorija, half-words, circular continuous mode. */
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

  /* Sampling time 47.5 ADC clk ciklusa @ 42.5 MHz = 1.12 us po kanalu.
   * 4 kanala x (47.5+12.5)/42.5 MHz = 5.65 us ukupni scan << 31.25 us TIM3 perioda.
   * Nužno za elektret mikrofone s izlaznom impedancijom ~1-10 kOhm. */
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_5);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_5, LL_ADC_SAMPLINGTIME_47CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_5, LL_ADC_SINGLE_ENDED);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, LL_ADC_CHANNEL_6);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_6, LL_ADC_SAMPLINGTIME_47CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_6, LL_ADC_SINGLE_ENDED);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, LL_ADC_CHANNEL_7);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_7, LL_ADC_SAMPLINGTIME_47CYCLES_5);
  LL_ADC_SetChannelSingleDiff(ADC1, LL_ADC_CHANNEL_7, LL_ADC_SINGLE_ENDED);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, LL_ADC_CHANNEL_8);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_8, LL_ADC_SAMPLINGTIME_47CYCLES_5);
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

/* --------------------------------------------------------------------------
 * Pomoćne funkcije detekcije
 * -------------------------------------------------------------------------- */
static void detect_reset(void)
{
  state        = ST_FILL;
  nf_energy    = 0U;
  blocks_seen  = 0U;
  hist_wr      = 0U;
  post_left    = 0U;
  refr_left    = 0U;
  assemble_req = false;
  event_ready  = false;
  event_count  = 0U;
  dropped_events = 0U;
  for (uint32_t ch = 0U; ch < ADC_NUM_CHANNELS; ch++)
  {
    dc[ch] = 2048;   /* sredina 12-bit skale: razuman početni DC za elektrete */
  }
}

/* Kopiraj jedan gotov DMA block u povijesni prsten, ažuriraj DC follower po
 * kanalu i vrati energiju bloka (suma kvadrata AC uzoraka). */
static uint64_t ingest_block(const uint16_t *src, uint32_t *out_ring_idx)
{
  const uint32_t idx = hist_wr;
  uint16_t      *dst = &hist[idx * ADC_BLOCK_LEN];
  uint64_t       acc = 0U;

  for (uint32_t s = 0U; s < ADC_BLOCK_SAMPLES; s++)
  {
    for (uint32_t ch = 0U; ch < ADC_NUM_CHANNELS; ch++)
    {
      int32_t x = (int32_t)(*src++);
      int32_t d = x - dc[ch];
      dc[ch] += (d >> EVENT_DC_SHIFT);          /* spori DC follower */
      acc    += (uint64_t)((int64_t)d * (int64_t)d);
      *dst++  = (uint16_t)x;                     /* spremi sirovi uzorak */
    }
  }

  hist_wr = (idx + 1U) & HIST_MASK;
  if (blocks_seen != 0xFFFFFFFFU)
  {
    blocks_seen++;
  }
  *out_ring_idx = idx;
  return acc;
}

static void nf_update(uint64_t e)
{
  int64_t delta = (int64_t)e - (int64_t)nf_energy;
  nf_energy = (uint64_t)((int64_t)nf_energy + (delta >> EVENT_NF_SHIFT));
}

/* Obradi jedan gotov block: napuni prsten, pokreni detection state machine. */
static void process_block(const uint16_t *src)
{
  uint32_t this_block;
  uint64_t e = ingest_block(src, &this_block);

  switch (state)
  {
    case ST_FILL:
      if (blocks_seen == 1U)
      {
        nf_energy = e;             /* seed */
      }
      else
      {
        nf_update(e);
      }
      /* Naoružan kad je šumni pod naučen i prsten drži dovoljno pre-roll
       * uzoraka za izgradnju punog prozora. */
      if (blocks_seen >= EVENT_NF_SEED_BLOCKS)
      {
        state = ST_IDLE;
      }
      break;

    case ST_IDLE:
    {
      uint64_t thr = nf_energy * (uint64_t)EVENT_TRIG_FACTOR;
      if ((e > thr) && (e > (uint64_t)EVENT_ABS_MIN_ENERGY))
      {
        trig_block = this_block;
        post_left  = EVENT_POST_BLOCKS;
        state      = ST_CAPTURE;
      }
      else
      {
        nf_update(e);              /* prilagođavaj samo dok je tiho */
      }
      break;
    }

    case ST_CAPTURE:
      if (post_left > 0U)
      {
        post_left--;
      }
      if (post_left == 0U)
      {
        /* Najstariji block prozora = trigger - PRE (omotano). */
        win_start_block = (trig_block + HIST_BLOCKS - EVENT_PRE_BLOCKS) & HIST_MASK;
        if (event_ready || assemble_req)
        {
          dropped_events++;        /* prethodni događaj još nije poslan */
        }
        else
        {
          assemble_req = true;
        }
        refr_left = EVENT_REFRACTORY_BLOCKS;
        state     = ST_REFRACTORY;
      }
      break;

    case ST_REFRACTORY:
      nf_update(e);
      if (refr_left > 0U)
      {
        refr_left--;
      }
      if (refr_left == 0U)
      {
        state = ST_IDLE;
      }
      break;

    default:
      state = ST_FILL;
      break;
  }
}

/* --------------------------------------------------------------------------
 * Javni API
 * -------------------------------------------------------------------------- */
void AdcCapture_Init(void)
{
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

  detect_reset();

  LL_TIM_DisableCounter(TIM3);
  LL_TIM_SetCounter(TIM3, 0U);

  LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)dma_buf);
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, 2U * ADC_BLOCK_LEN);
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

void AdcCapture_Task(void)
{
  if (!assemble_req)
  {
    return;
  }

  /* Snimi zahtjev, pa kopiraj prozor iz povijesnog prstena. Prsten se i dalje
   * upisuje iza nas, ali ima zalihe (HIST_BLOCKS - EVENT_WIN_BLOCKS = 15
   * blokova / ~30 ms) prije nego bi se najstariji block prozora prepisao. */
  uint32_t b = win_start_block;
  assemble_req = false;

  for (uint32_t i = 0U; i < FRAME_SYNC_LEN; i++)
  {
    tx_packet[i] = FRAME_SYNC_U16;
  }

  uint16_t *out = &tx_packet[FRAME_SYNC_LEN];
  for (uint32_t blk = 0U; blk < EVENT_WIN_BLOCKS; blk++)
  {
    memcpy(out, &hist[b * ADC_BLOCK_LEN], ADC_BLOCK_LEN * sizeof(uint16_t));
    out += ADC_BLOCK_LEN;
    b = (b + 1U) & HIST_MASK;
  }

  event_count++;
  event_ready = true;
}

bool AdcCapture_EventReady(void)
{
  return event_ready;
}

const uint16_t *AdcCapture_EventPacket(void)
{
  return tx_packet;
}

void AdcCapture_EventConsumed(void)
{
  event_ready = false;
}

uint32_t AdcCapture_EventCount(void)
{
  return event_count;
}

uint32_t AdcCapture_DroppedEvents(void)
{
  return dropped_events;
}

void AdcCapture_DmaIrqHandler(void)
{
  if (LL_DMA_IsActiveFlag_HT1(DMA1))
  {
    LL_DMA_ClearFlag_HT1(DMA1);
    process_block(&dma_buf[0]);                 /* prvi block gotov */
  }

  if (LL_DMA_IsActiveFlag_TC1(DMA1))
  {
    LL_DMA_ClearFlag_TC1(DMA1);
    process_block(&dma_buf[ADC_BLOCK_LEN]);     /* drugi block gotov */
  }

  if (LL_DMA_IsActiveFlag_TE1(DMA1))
  {
    LL_DMA_ClearFlag_TE1(DMA1);
    /* Transfer error: odbaci stanje detekcije, ponovno nauči šumni pod. */
    state       = ST_FILL;
    blocks_seen = 0U;
  }
}
