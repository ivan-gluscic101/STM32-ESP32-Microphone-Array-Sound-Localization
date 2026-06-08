#ifndef ADC_CAPTURE_H
#define ADC_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

/* Slobodno-tekući 4-kanalni ADC s detekcijom pljeska na čipu.
 *
 * Korištenje iz glavne petlje:
 *   AdcCapture_Init();
 *   AdcCapture_Start();
 *   while (1) {
 *     AdcCapture_Task();                 // sastavi prozor događaja na čekanju
 *     if (AdcCapture_EventReady() && !UartStream_Busy()) {
 *       UartStream_Send(AdcCapture_EventPacket(), ADC_TX_BYTES);
 *       AdcCapture_EventConsumed();
 *     }
 *   }
 */
void            AdcCapture_Init(void);
void            AdcCapture_Start(void);

/* Poziva se u glavnoj petlji. Kopira uhvaćeni prozor iz povijesnog prstena u
 * TX paket kad je ISR označio dovršen događaj. Jeftino i bez efekta (no-op)
 * kad ništa nije na čekanju. */
void            AdcCapture_Task(void);

bool            AdcCapture_EventReady(void);
const uint16_t *AdcCapture_EventPacket(void);   /* sync zaglavlje + ADC_FRAME_LEN uzoraka */
void            AdcCapture_EventConsumed(void);  /* pozovi kad je paket poslan */

uint32_t        AdcCapture_EventCount(void);     /* dijagnostika */
uint32_t        AdcCapture_DroppedEvents(void);  /* događaji izgubljeni jer je TX bio zauzet */

void            AdcCapture_DmaIrqHandler(void);  /* zovi iz DMA1_Channel1_IRQHandler */

#endif /* ADC_CAPTURE_H */
