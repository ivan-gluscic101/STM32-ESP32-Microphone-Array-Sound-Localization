#ifndef ADC_CAPTURE_H
#define ADC_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

void AdcCapture_Init(void);
void AdcCapture_Start(void);
bool AdcCapture_FrameReady(void);
const uint16_t *AdcCapture_Buffer(void);
uint32_t AdcCapture_DroppedFrames(void);
void AdcCapture_DmaIrqHandler(void);

#endif /* ADC_CAPTURE_H */
