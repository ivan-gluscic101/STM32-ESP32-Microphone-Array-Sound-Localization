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

/*
 * UART_SendRawCapture — pošalji sirove (deinterleaveane, poravnate) uzorke sva
 * 4 kanala na ESP32 kao binarni okvir (tip 0x04), nakon detekcije. Blocking.
 *
 * Okvir:
 *   [0xAA][0xBB][0x04][NCH][N_H][N_L]  zaglavlje
 *     NCH = broj kanala (NUM_CH),  N = uzoraka po kanalu (SAMPLES_PER_CHANNEL),
 *     big-endian
 *   zatim NCH × N uzoraka, big-endian uint16, kanal-major redoslijed:
 *     ch0[0..N-1], ch1[0..N-1], ch2[0..N-1], ch3[0..N-1]
 *   [0xCC][0xDD]  završetak
 *
 * Svaki niz mora imati SAMPLES_PER_CHANNEL uint16 uzoraka.
 */
void UART_SendRawCapture(const uint16_t *ch0, const uint16_t *ch1,
                         const uint16_t *ch2, const uint16_t *ch3);

#endif /* UART_DRIVER_H */
