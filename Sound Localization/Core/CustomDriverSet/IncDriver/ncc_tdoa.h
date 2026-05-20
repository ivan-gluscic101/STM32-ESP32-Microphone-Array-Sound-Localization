#ifndef NCC_TDOA_H
#define NCC_TDOA_H

/*
 * ncc_tdoa.h — Normalized Cross-Correlation za estimaciju TDOA (rezervna impl.)
 *
 * Napomena: aktivna implementacija koristi gcc_phat.h / gcc_phat.c.
 * Ove funkcije nisu pozivane iz pipeline-a i mogu se koristiti za usporedbu.
 *
 * Implementacija: vremenska domena, O(N × 2L) po paru, globalna normalizacija.
 *   N = SAMPLES_PER_CHANNEL = 512, L = TDOA_MAX_SAMPLES = 20
 *
 * ADC buffer layout (interleaved):
 *   buf[s*4 + 0] = M1 (PA0,  RANK1)
 *   buf[s*4 + 1] = M2 (PB14, RANK2)
 *   buf[s*4 + 2] = M3 (PC0,  RANK3)
 *   buf[s*4 + 3] = M4 (PC1,  RANK4)
 */

#include "audio_common.h"
#include <stdint.h>

/*
 * Deinterleava half-buffer u 4 odvojena float niza i uklanja DC po kanalu.
 */
void NCC_ExtractChannels(const uint16_t *buf,
                         float ch0[], float ch1[],
                         float ch2[], float ch3[]);

/* RMS jednog kanala (SAMPLES_PER_CHANNEL uzoraka, DC već uklonjen). */
float NCC_RMS(const float *ch);

/*
 * Normalizirana cross-korelacija u vremenskoj domeni.
 * Vraća integer lag u [-TDOA_MAX_SAMPLES, +TDOA_MAX_SAMPLES].
 * Pozitivni lag = ref vodi sig.
 */
int32_t NCC_ComputeLag(const float *ref, const float *sig);

#endif /* NCC_TDOA_H */
