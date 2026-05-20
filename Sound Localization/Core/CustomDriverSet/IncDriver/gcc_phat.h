#ifndef GCC_PHAT_H
#define GCC_PHAT_H

/*
 * gcc_phat.h — Normalizirana križna korelacija (GCC bez PHAT težiniranja)
 *
 * Implementacija: vremenska domena, O(N × 2L) po paru
 *   N = SAMPLES_PER_CHANNEL = 512, L = TDOA_MAX_SAMPLES = 20
 *   512 × 40 × 3 para × 4B ≈ ~250 K float operacija → ~1.5 ms @ 170 MHz FPU
 *
 * Za PHAT težiniranje (bolje u reverberantnim prostorima) potreban je
 * arm_rfft_fast_f32 iz CMSIS-DSP — može se dodati naknadno.
 *
 * ADC buffer layout (interleaved):
 *   buf[s*4 + 0] = M1 (PA0,  RANK1) — uzorkovan u t_s + 0×870.6 ns
 *   buf[s*4 + 1] = M2 (PB14, RANK2) — uzorkovan u t_s + 1×870.6 ns
 *   buf[s*4 + 2] = M3 (PC0,  RANK3) — uzorkovan u t_s + 2×870.6 ns
 *   buf[s*4 + 3] = M4 (PC1,  RANK4) — uzorkovan u t_s + 3×870.6 ns
 */

#include "audio_common.h"
#include <stdint.h>

/*
 * Deinterleava SAMPLES_PER_CHANNEL frejmova u 4 odvojena float niza i uklanja
 * DC po kanalu. Početak prozora je na frejmu `frame_offset` unutar `buf` —
 * podržava sliding/overlap obradu unutar većeg buffera (npr. 1024 frejma).
 *
 * Pozivatelj mora osigurati da `buf` sadrži barem (frame_offset + SAMPLES_PER_CHANNEL)
 * frejmova × NUM_CH uzoraka.
 */
void GCC_ExtractChannels(const uint16_t *buf, uint32_t frame_offset,
                         float ch0[], float ch1[],
                         float ch2[], float ch3[]);

/*
 * Normalizirana križna korelacija s paraboličkom interpolacijom vrha.
 * Vraća sub-sample lag u [-TDOA_MAX, +TDOA_MAX] (float).
 * Pozitivni lag = ref vodi sig (ref dolazi prije sig u prostoru).
 */
float GCC_ComputeLag(const float *ref, const float *sig);

/* RMS jednog kanala (SAMPLES_PER_CHANNEL uzoraka, DC već uklonjen). */
float GCC_RMS(const float *ch);

#endif /* GCC_PHAT_H */
