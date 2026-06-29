#ifndef RAW_CAPTURE_H
#define RAW_CAPTURE_H

/*
 * raw_capture.h — snapshot sirovih ADC uzoraka za UART debug / MATLAB export.
 *
 * Neovisno o lokalizacijskoj verziji (3-mic / 4-mic time-domain): obje pune iste
 * dbg_raw_chX nizove pri detekciji, a UART_Task ih ispisuje kao CSV (Type 0x04).
 *
 * ADC buffer layout (interleaved):
 *   buf[s*4 + 0] = M1 (PB14, RANK1)
 *   buf[s*4 + 1] = M2 (PC0,  RANK2)
 *   buf[s*4 + 2] = M3 (PC1,  RANK3)
 *   buf[s*4 + 3] = M4 (PC2,  RANK4)
 */

#include "audio_common.h"
#include <stdint.h>

/* Snima sirovi sadržaj kanala u dbg_raw_chX[]. Zovi samo kad treba (npr. pri
 * detekciji), ne za svaki prozor — inače su debug nizovi uvijek od zadnjeg
 * prozora, ne od onog koji je trigerirao detekciju.
 * frame_offset omogućava sliding window unutar većeg buffera. */
void RawCapture_Snapshot(const uint16_t *buf, uint32_t frame_offset);

/* Debug snapshot (zadnji detektirani prozor po kanalu). */
extern uint16_t dbg_raw_ch0[SAMPLES_PER_CHANNEL];
extern uint16_t dbg_raw_ch1[SAMPLES_PER_CHANNEL];
extern uint16_t dbg_raw_ch2[SAMPLES_PER_CHANNEL];
extern uint16_t dbg_raw_ch3[SAMPLES_PER_CHANNEL];

#endif /* RAW_CAPTURE_H */
