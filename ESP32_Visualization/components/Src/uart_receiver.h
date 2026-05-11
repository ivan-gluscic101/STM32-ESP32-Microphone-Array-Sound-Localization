#ifndef __UART_RECEIVER_H
#define __UART_RECEIVER_H

#include "esp_err.h"
#include "driver/uart.h"

// Funkcija za inicijalizaciju UART prijemnika i pokretanje taska
void uart_receiver_init(void);

#endif // __UART_RECEIVER_H