#include "gcc_phat.h"
#include <math.h>

void GCC_ExtractChannels(const uint16_t *buf, uint32_t frame_offset,
                         float ch0[], float ch1[],
                         float ch2[], float ch3[])
{
    const uint16_t *p = &buf[frame_offset * NUM_CH];

    /* Prolaz 1: akumuliraj DC po kanalu */
    float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f;
    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        sum0 += (float)p[s * NUM_CH + 0];
        sum1 += (float)p[s * NUM_CH + 1];
        sum2 += (float)p[s * NUM_CH + 2];
        sum3 += (float)p[s * NUM_CH + 3];
    }
    float inv_n = 1.0f / SAMPLES_PER_CHANNEL;
    float dc0 = sum0 * inv_n;
    float dc1 = sum1 * inv_n;
    float dc2 = sum2 * inv_n;
    float dc3 = sum3 * inv_n;

    /* Prolaz 2: deinterleave + DC removal */
    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        ch0[s] = (float)p[s * NUM_CH + 0] - dc0;
        ch1[s] = (float)p[s * NUM_CH + 1] - dc1;
        ch2[s] = (float)p[s * NUM_CH + 2] - dc2;
        ch3[s] = (float)p[s * NUM_CH + 3] - dc3;
    }
}

float GCC_RMS(const float *ch)
{
    float acc = 0.0f;
    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        acc += ch[s] * ch[s];
    }
    return sqrtf(acc / (float)SAMPLES_PER_CHANNEL);
}

float GCC_ComputeLag(const float *ref, const float *sig, float *out_peak)
{
    /* Globalna normalizacija: e_ref × e_sig (obračunato jednom za sve lagove).
     * Za lagove << N greška je < 4%, prihvatljivo za detekciju vrha. */
    float e_ref = 0.0f, e_sig = 0.0f;
    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        e_ref += ref[s] * ref[s];
        e_sig += sig[s] * sig[s];
    }
    float norm = sqrtf(e_ref * e_sig);
    if (norm < 1e-6f) {
        if (out_peak) *out_peak = 0.0f;
        return 0.0f;
    }

    /* Prolaz 1: izračunaj korelacije za sve lagove i pohrani ih.
     * Indeks u polju: i = lag + TDOA_MAX_SAMPLES  (raspon 0 … 2×TDOA_MAX_SAMPLES)
     * Veličina fiksna (41 × 4 B = 164 B na stacku). */
#define N_LAGS (2 * TDOA_MAX_SAMPLES + 1)
    float corr[N_LAGS];
    int   best_idx = 0;
    float best_val = -2.0f;

    for (int i = 0; i < (int)N_LAGS; i++) {
        int32_t lag  = (int32_t)i - TDOA_MAX_SAMPLES;
        int s_start  = (lag < 0) ? -lag : 0;
        int s_end    = (lag > 0) ? SAMPLES_PER_CHANNEL - lag : SAMPLES_PER_CHANNEL;

        float acc = 0.0f;
        for (int s = s_start; s < s_end; s++) {
            acc += ref[s] * sig[s + lag];
        }
        corr[i] = acc / norm;

        if (corr[i] > best_val) {
            best_val = corr[i];
            best_idx = i;
        }
    }

    if (out_peak) *out_peak = best_val;

    /* Prolaz 2: parabolička interpolacija oko vrha za sub-sample preciznost.
     * Parabola kroz tri točke:
     *   delta = 0.5 * (R[i-1] - R[i+1]) / (R[i-1] - 2*R[i] + R[i+1])
     * Primjenjiva samo kad postoje susjedi (vrh nije na rubu raspona). */
    if (best_idx > 0 && best_idx < (int)N_LAGS - 1) {
        float rm    = corr[best_idx - 1];
        float rp    = corr[best_idx + 1];
        float denom = rm - 2.0f * best_val + rp;
        if (denom < -1e-6f) {   /* negativni denom = pravi konkavni vrh */
            float delta = 0.5f * (rm - rp) / denom;
            return (float)(best_idx - TDOA_MAX_SAMPLES) + delta;
        }
    }

    return (float)(best_idx - TDOA_MAX_SAMPLES);
}
