#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include "main.h"
#include "audio_common.h"
#include <stdint.h>

#define FRAME_SOF1    ((uint8_t)0xAA)
#define FRAME_SOF2    ((uint8_t)0xBB)
#define FRAME_EOF1    ((uint8_t)0xCC)
#define FRAME_EOF2    ((uint8_t)0xDD)

/* Svaki FRAME_SKIP half-buffer eventa šaljemo jedan audio frame.
 * Half-buffer = 512 uzoraka @ 64 kHz = 8 ms.
 * FRAME_SKIP=16 → jedan frame svakih 128 ms (~7.8 FPS).
 * Frame = 4102 B → ~44.5 ms @ 921600 baud < 128 ms. */
#define FRAME_SKIP    16

typedef struct {
    int16_t  phi_tenth_deg;
    uint8_t  strength;
    uint8_t  angle_valid;
#if SEND_AUDIO_FRAMES
    uint16_t audio_snapshot[HALF_BUFFER];
    uint8_t  send_audio;
#endif
} uart_msg_t;

void Custom_UART4_Init(void);
void UART_SendFrame(const uint16_t *buf, uint32_t samples_per_ch, uint16_t fid);
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
