#ifndef GCC_PHAT_H
#define GCC_PHAT_H

/*
 * gcc_phat.h — GCC-PHAT (Generalized Cross-Correlation with Phase Transform)
 *
 * Implementacija: FFT domena preko CMSIS-DSP arm_rfft_fast_f32
 *   FFT_SIZE = SAMPLES_PER_CHANNEL = 512
 *   Po paru:  2 × forward FFT + cross-spektar + PHAT normalizacija + 1 × IFFT
 *
 * ADC buffer layout (interleaved):
 *   buf[s*4 + 0] = M1 (PA0, RANK1)  — uzorkovan u t_s + 0 × CH_DELAY_S
 *   buf[s*4 + 1] = M2 (PA1, RANK2)  — uzorkovan u t_s + 1 × CH_DELAY_S
 *   buf[s*4 + 2] = M3 (PC0, RANK3)  — uzorkovan u t_s + 2 × CH_DELAY_S
 *   buf[s*4 + 3] = M4 (PC1, RANK4)  — uzorkovan u t_s + 3 × CH_DELAY_S
 */

#include "audio_common.h"
#include <stdint.h>

/* Inicijalizacija FFT instance + Hann prozora. Pozvati JEDNOM na startu. */
void GCC_Init(void);

/* Deinterleave + DC removal + Hann prozor.
 * frame_offset omogućava sliding window unutar većeg buffera. */
void GCC_ExtractChannels(const uint16_t *buf, uint32_t frame_offset,
                         float ch0[], float ch1[],
                         float ch2[], float ch3[]);

/* GCC-PHAT između dva signala. corr duljine SAMPLES_PER_CHANNEL.
 * Pozitivni indeks vrha (< N/2) = sig kasni za ref-om. */
void GCC_PHAT(const float *ref, const float *sig, float *corr);

/* Vrh korelacije + parabolička interpolacija → kašnjenje u sekundama.
 * Pozitivno = sig kasni za ref-om.
 * qual_out (nullable): peak / mean|corr| — visoko za pravi signal, ~1 za šum. */
float GCC_FindTDOA(const float *corr, float *qual_out);

/* RMS jednog kanala (DC već uklonjen). */
float GCC_RMS(const float *ch);

/* Debug snapshot. */
extern uint16_t dbg_raw_ch0[SAMPLES_PER_CHANNEL];
extern uint16_t dbg_raw_ch1[SAMPLES_PER_CHANNEL];
extern uint16_t dbg_raw_ch2[SAMPLES_PER_CHANNEL];
extern uint16_t dbg_raw_ch3[SAMPLES_PER_CHANNEL];
extern float    dbg_dc[4];

#endif /* GCC_PHAT_H */
