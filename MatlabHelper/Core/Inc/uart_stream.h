/**
  ******************************************************************************
  * @file    uart_stream.h
  * @brief   USART3 binary streaming over DMA (LL drivers).
  *
  *          Sends a raw byte buffer to MATLAB at 921600 baud, 8N1. The frame
  *          is transmitted via DMA1_Channel2 so the CPU is free meanwhile.
  ******************************************************************************
  */
#ifndef UART_STREAM_H
#define UART_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Configure USART3 (PB10 TX / PB11 RX) and its TX DMA channel. */
void UartStream_Init(void);

/* True while a DMA transmission is in progress. */
bool UartStream_Busy(void);

/* Start a DMA transmission of 'len' bytes. Ignored if already busy. */
void UartStream_Send(const uint8_t *data, uint16_t len);

/* Must be called from DMA1_Channel2_IRQHandler. */
void UartStream_DmaIrqHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_STREAM_H */
