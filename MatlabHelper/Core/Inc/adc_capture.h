/**
  ******************************************************************************
  * @file    adc_capture.h
  * @brief   Single-shot 4-channel ADC acquisition (LL drivers).
  *
  *          TIM3 TRGO triggers a 4-rank scan at SAMPLE_RATE_HZ. DMA1_Channel1
  *          stores the interleaved 12-bit samples into an internal frame
  *          buffer. When ADC_FRAME_LEN samples are collected the timer is
  *          stopped, so the buffer is stable while it is being streamed out.
  ******************************************************************************
  */
#ifndef ADC_CAPTURE_H
#define ADC_CAPTURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Configure GPIO analog pins, TIM3, ADC1 + DMA and run ADC calibration.
 * Leaves the ADC enabled and idle (no conversions running). */
void AdcCapture_Init(void);

/* Arm DMA and start TIM3 to acquire exactly one frame. */
void AdcCapture_Start(void);

/* True once a full frame (ADC_FRAME_LEN samples) has been captured. */
bool AdcCapture_FrameReady(void);

/* Pointer to the TX buffer: leading sync header followed by the captured
 * interleaved frame ([0xFFFF 0xFFFF][CH1 CH2 CH3 CH4] ...). Send ADC_TX_BYTES. */
const uint16_t *AdcCapture_Buffer(void);

/* Must be called from DMA1_Channel1_IRQHandler. */
void AdcCapture_DmaIrqHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_CAPTURE_H */
