#include "ncc_tdoa.h"
#include <math.h>

/*
 * NCC_FindTDOA — time-domain cross-correlation TDOA estimator.
 *
 * Ulaz: dva DC-removed + Hann-windowed kanala (izlaz GCC_ExtractChannels).
 * Izlaz: kašnjenje u sekundama (+ = sig kasni za ref).
 *
 * Ključne odluke:
 *   - Apsolutni peak: hvata invertiran polaritet kanala (neg. korelacijski vrh).
 *   - Boundary-aware overlap: za lag τ korelira samo preklapajuće uzorke
 *     [max(0,-τ) … min(N,N-τ)) — nema padding ili zero-extend.
 *   - Normalizacija po energiji: dijeli s sqrt(E_ref * E_sig) → corr ∈ [-1,+1].
 *   - Quality = |peak_corr| / mean(|corr|) po cijelom lag prozoru.
 *   - Parabolička interpolacija s cirkularnim susjedima za sub-sample preciznost.
 */
float NCC_FindTDOA(const float *ref, const float *sig, float *qual_out)
{
    const int N       = SAMPLES_PER_CHANNEL;
    const int MAX_LAG = TDOA_MAX_SAMPLES;
    const int N_LAGS  = 2 * MAX_LAG + 1;

    /* Globalna normalizacija: sqrt(E_ref × E_sig) */
    float e_ref = 0.0f, e_sig = 0.0f;
    for (int s = 0; s < N; s++) {
        e_ref += ref[s] * ref[s];
        e_sig += sig[s] * sig[s];
    }
    float norm = sqrtf(e_ref * e_sig);
    if (norm < 1e-9f) {
        if (qual_out) *qual_out = 0.0f;
        return 0.0f;
    }
    float inv_norm = 1.0f / norm;

    /* Izračun xcorr za svaki lag i pohrana za interpolaciju + quality */
    float xcorr[N_LAGS];
    float best_abs = 0.0f;
    int   best_idx = MAX_LAG;   /* sredina niza = lag 0 */

    for (int lag = -MAX_LAG; lag <= MAX_LAG; lag++) {
        /* Valjani preklapajući raspon — nema zero-paddinga */
        int s0 = (lag < 0) ? -lag : 0;
        int s1 = (lag > 0) ? N - lag : N;

        float acc = 0.0f;
        for (int s = s0; s < s1; s++) {
            acc += ref[s] * sig[s + lag];
        }
        /* Normalizacija: energija × korak za unbiased estimaciju */
        acc *= inv_norm;

        int idx     = lag + MAX_LAG;
        xcorr[idx]  = acc;
        float a     = fabsf(acc);
        if (a > best_abs) {
            best_abs = a;
            best_idx = idx;
        }
    }

    /* Quality: |peak| / mean(|xcorr|) */
    if (qual_out) {
        float sum = 0.0f;
        for (int i = 0; i < N_LAGS; i++) sum += fabsf(xcorr[i]);
        float mean = sum / (float)N_LAGS;
        *qual_out = (mean > 1e-9f) ? (best_abs / mean) : 0.0f;
    }

    /* Parabolička interpolacija — radi na apsolutnim vrijednostima da uvijek
     * interpolira prema vrhu, bez obzira na predznak (invertiran kanal). */
    float delta = 0.0f;
    if (best_idx > 0 && best_idx < N_LAGS - 1) {
        float c0 = fabsf(xcorr[best_idx - 1]);
        float c1 = best_abs;
        float c2 = fabsf(xcorr[best_idx + 1]);
        float denom = c0 - 2.0f * c1 + c2;
        if (fabsf(denom) > 1e-12f) {
            delta = 0.5f * (c0 - c2) / denom;
            if (delta >  1.0f) delta =  1.0f;
            if (delta < -1.0f) delta = -1.0f;
        }
    }

    float lag_f = (float)(best_idx - MAX_LAG) + delta;
    return lag_f * SAMPLE_PERIOD_S;
}
