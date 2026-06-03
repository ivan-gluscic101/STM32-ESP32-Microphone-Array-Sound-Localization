#ifndef NCC_TDOA_H
#define NCC_TDOA_H

/*
 * ncc_tdoa.h — Time-domain cross-correlation TDOA estimator.
 *
 * Zamjena za GCC-PHAT pipeline (gcc_phat.c GCC_PHAT + GCC_FindTDOA).
 * Ekstrakcija kanala i RMS i dalje dolaze iz gcc_phat.h (GCC_ExtractChannels,
 * GCC_RMS) jer dijele DC-removal + Hann window logiku.
 */

#include "audio_common.h"
#include <stdint.h>

/*
 * NCC_FindTDOA — cross-correlira DC-removed+Hann-windowed kanale u vremenskoj
 * domeni i vraća kašnjenje u sekundama.
 *
 *   ref, sig  : izlaz GCC_ExtractChannels (float, SAMPLES_PER_CHANNEL uzoraka)
 *   qual_out  : (nullable) |peak_corr| / mean|corr| — visoko za pravi signal
 *   return    : τ u sekundama, pozitivno = sig kasni za ref
 *
 * Svojstva:
 *   - Apsolutni peak: ispravno radi i za invertiran polaritet kanala
 *   - Boundary-aware: korelira samo valjani preklapajući raspon za svaki lag
 *   - Parabolička interpolacija: sub-sample preciznost
 */
float NCC_FindTDOA(const float *ref, const float *sig, float *qual_out);

#endif /* NCC_TDOA_H */
