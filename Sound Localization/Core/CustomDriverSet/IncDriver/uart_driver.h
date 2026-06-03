#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include "main.h"
#include <stdint.h>

void Custom_UART4_Init(void);
void UART_SendAnglePacket(int16_t phi_tenth_deg, uint8_t strength);

/*
 * UART_SendAngle3DPacket — šalje 3D lokalizacijski paket (tip 0x03, 10 bajta)
 *
 * Format: [0xAA][0xBB][0x03][AZ_H][AZ_L][EL_H][EL_L][STR][0xCC][0xDD]
 *   AZ = azimut u 0.1°, big-endian int16  (-1800..+1800)
 *   EL = elevacija u 0.1°, big-endian int16 (-900..+900)
 *   STR = jakost 0-100
 */
void UART_SendAngle3DPacket(int16_t az_tenth, int16_t el_tenth, uint8_t strength);

#endif /* UART_DRIVER_H */
