#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include "driver/uart.h"

/* Konfiguracija pinova za ESP32 */
#define UART_PORT_NUM       UART_NUM_1
#define UART_TX_PIN         (17)
#define UART_RX_PIN         (16)
#define UART_BUF_SIZE       (1024)
#define UART_BAUD_RATE      (115200)

/* Markeri paketa (identično kao na STM32 strani) */
#define ANGLE_PKT_SOF1      0xAA
#define ANGLE_PKT_SOF2      0xBB
#define ANGLE_PKT_TYPE_2D   0x02  /* [AZ_H][AZ_L][STR] */
#define ANGLE_PKT_TYPE_3D   0x03  /* [AZ_H][AZ_L][EL_H][EL_L][STR] */
#define ANGLE_PKT_EOF1      0xCC
#define ANGLE_PKT_EOF2      0xDD
#define ANGLE_PKT_LEN_2D    8
#define ANGLE_PKT_LEN_3D    10

/**
 * @brief Inicijalizira UART periferiju za komunikaciju sa STM32.
 */
void uart_driver_init(void);

#endif // UART_DRIVER_H