#ifndef UART_STREAM_H
#define UART_STREAM_H

#include <stdbool.h>
#include <stdint.h>

void UartStream_Init(void);
bool UartStream_Busy(void);
void UartStream_Send(const uint8_t *data, uint16_t len);
void UartStream_DmaIrqHandler(void);

#endif /* UART_STREAM_H */
