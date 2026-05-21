#ifndef XCORR_H
#define XCORR_H

/*
 * xcorr.h — GCC-PHAT (Generalized Cross-Correlation with Phase Transform)
 *
 * Algoritam:
 *   1. Hann windowing + zero-padding do 2×N = 1024
 *   2. RFFT (arm_rfft_fast_f32, N=1024)
 *   3. Kros-spektar s PHAT težiniranjem: G[k] = conj(X[k])·Y[k] / |conj(X[k])·Y[k]|
 *      → svaki bin ima amplitudu 1 (samo faza ostaje)
 *   4. IRFFT → time-domain GCC-PHAT korelacija
 *   5. Peak u rasponu ±TDOA_MAX_SAMPLES → integer lag
 *
 * ADC buffer layout (interleaved):
 *   buf[s*4 + 0] = M1 (PA0,  RANK1) — uzorkovan u t_s + 0×870.6 ns
 *   buf[s*4 + 1] = M2 (PB14, RANK2) — uzorkovan u t_s + 1×870.6 ns
 *   buf[s*4 + 2] = M3 (PC0,  RANK3) — uzorkovan u t_s + 2×870.6 ns
 *   buf[s*4 + 3] = M4 (PC1,  RANK4) — uzorkovan u t_s + 3×870.6 ns
 */

#include "audio_common.h"
#include <stdint.h>

/* Jednom na startu — inicijalizira RFFT instancu i preračunava Hann prozor. */
void XCorr_Init(void);

/*
 * Deinterleava half-buffer u 4 odvojena float niza i uklanja DC po kanalu.
 * Svi izlazni nizovi moraju imati SAMPLES_PER_CHANNEL elemenata.
 */
void XCorr_ExtractChannels(const uint16_t *buf,
                           float ch0[], float ch1[],
                           float ch2[], float ch3[]);

/*
 * Windowing + FFT referentnog kanala — pohrani interno.
 * Mora se pozvati jednom po frameu PRIJE XCorr_ComputeLag.
 */
void XCorr_PrepareRef(const float *ref);

/*
 * GCC-PHAT lag u odnosu na cacheiran referentni kanal.
 * Vraća integer lag u [-TDOA_MAX_SAMPLES, +TDOA_MAX_SAMPLES].
 */
int32_t XCorr_ComputeLag(const float *sig);

/* RMS jednog kanala (SAMPLES_PER_CHANNEL uzoraka, DC već uklonjen). */
float XCorr_RMS(const float *ch);

#endif /* XCORR_H */
