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
 * Deinterleava half-buffer u 4 odvojena float niza i uklanja DC po kanalu.
 * Svi izlazni nizovi moraju imati SAMPLES_PER_CHANNEL elemenata.
 */
void GCC_ExtractChannels(const uint16_t *buf,
                         float ch0[], float ch1[],
                         float ch2[], float ch3[]);

/*
 * Normalizirana križna korelacija — vraća integer lag u [-TDOA_MAX, +TDOA_MAX].
 * Pozitivni lag = ref vodi sig (ref dolazi prije sig u prostoru).
 */
int32_t GCC_ComputeLag(const float *ref, const float *sig);

/* RMS jednog kanala (SAMPLES_PER_CHANNEL uzoraka, DC već uklonjen). */
float GCC_RMS(const float *ch);

#endif /* GCC_PHAT_H */
