/**
  ******************************************************************************
  * @file    app_config.h
  * @brief   Shared acquisition parameters for the ADC delay-measurement
  *          experiment. These values MUST stay in sync with DelayProcessing.m.
  ******************************************************************************
  */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Acquisition geometry
 *
 * The 4 ADC ranks are scanned sequentially on every TIM3 trigger:
 *   rank1 -> CH5 (PB14)  = MATLAB CH1   (reference)
 *   rank2 -> CH6 (PC0)   = MATLAB CH2
 *   rank3 -> CH7 (PC1)   = MATLAB CH3
 *   rank4 -> CH8 (PC2)   = MATLAB CH4
 *
 * For the experiment every microphone is wired in parallel to all 4 inputs,
 * so the only delay between channels should be the sequential ADC scan delay
 * of (2.5 + 12.5) ADC clock cycles measured by DelayProcessing.m.
 * ------------------------------------------------------------------------- */
#define ADC_NUM_CHANNELS      4U
#define SAMPLES_PER_CHANNEL   1024U   /* must match SAMPLES_PER_CH in the .m */

/* One frame = interleaved [CH1 CH2 CH3 CH4] x SAMPLES_PER_CHANNEL */
#define ADC_FRAME_LEN         (ADC_NUM_CHANNELS * SAMPLES_PER_CHANNEL) /* 4096 u16 */
#define ADC_FRAME_BYTES       (ADC_FRAME_LEN * 2U)                     /* 8192 B    */

/* Frame sync marker prepended to every UART frame so MATLAB can lock onto the
 * correct byte/channel alignment in the free-running stream.
 *
 * The payload is 12-bit ADC data: every high byte is <= 0x0F, so a run of
 * 0xFF bytes can never occur inside the payload. Two 0xFFFF words = 4x 0xFF
 * is therefore a collision-free sync pattern. */
#define FRAME_SYNC_U16        0xFFFFU
#define FRAME_SYNC_LEN        2U                              /* 2 u16 = 4 sync bytes */
#define ADC_TX_LEN            (FRAME_SYNC_LEN + ADC_FRAME_LEN)
#define ADC_TX_BYTES          (ADC_TX_LEN * 2U)               /* 4 + 8192 = 8196 B    */

/* Per-channel sample rate -> TIM3 update (TRGO) frequency. */
#define SAMPLE_RATE_HZ        64000U

/* TIM3 runs from the 170 MHz APB1 timer clock. ARR = f_tim/Fs - 1. */
#define TIM3_CLOCK_HZ         170000000U
#define TIM3_AUTORELOAD       ((TIM3_CLOCK_HZ / SAMPLE_RATE_HZ) - 1U)  /* 2655 -> 64.006 kHz */

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
