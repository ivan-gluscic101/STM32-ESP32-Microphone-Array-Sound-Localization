#include "gcc_phat.h"
#include "arm_math.h"
#include <math.h>

#define FFT_SIZE        SAMPLES_PER_CHANNEL

#ifndef PI
#define PI 3.14159265358979323846f
#endif

uint16_t dbg_raw_ch0[FFT_SIZE];
uint16_t dbg_raw_ch1[FFT_SIZE];
uint16_t dbg_raw_ch2[FFT_SIZE];
uint16_t dbg_raw_ch3[FFT_SIZE];
float    dbg_dc[4];

static arm_rfft_fast_instance_f32 s_rfft;
static float s_hann[FFT_SIZE];

void GCC_Init(void)
{
    for (int i = 0; i < FFT_SIZE; i++) {
        s_hann[i] = 0.5f * (1.0f - cosf(2.0f * PI * (float)i / (float)(FFT_SIZE - 1)));
    }
    arm_rfft_fast_init_f32(&s_rfft, FFT_SIZE);
}

void GCC_ExtractChannels(const uint16_t *buf, uint32_t frame_offset,
                         float ch0[], float ch1[],
                         float ch2[], float ch3[])
{
    const uint16_t *p = &buf[frame_offset * NUM_CH];

    float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f;
    for (int s = 0; s < FFT_SIZE; s++) {
        sum0 += (float)p[s * NUM_CH + 0];
        sum1 += (float)p[s * NUM_CH + 1];
        sum2 += (float)p[s * NUM_CH + 2];
        sum3 += (float)p[s * NUM_CH + 3];
    }
    float inv_n = 1.0f / (float)FFT_SIZE;
    float dc0 = sum0 * inv_n;
    float dc1 = sum1 * inv_n;
    float dc2 = sum2 * inv_n;
    float dc3 = sum3 * inv_n;

    dbg_dc[0] = dc0; dbg_dc[1] = dc1; dbg_dc[2] = dc2; dbg_dc[3] = dc3;

    for (int s = 0; s < FFT_SIZE; s++) {
        uint16_t r0 = p[s * NUM_CH + 0];
        uint16_t r1 = p[s * NUM_CH + 1];
        uint16_t r2 = p[s * NUM_CH + 2];
        uint16_t r3 = p[s * NUM_CH + 3];

        float w = s_hann[s];
        ch0[s] = ((float)r0 - dc0) * w;
        ch1[s] = ((float)r1 - dc1) * w;
        ch2[s] = ((float)r2 - dc2) * w;
        ch3[s] = ((float)r3 - dc3) * w;
    }
}

/* Snima sirovi sadržaj kanala u dbg_raw_chX[]. Zovi samo kad treba (npr. pri
 * detekciji), ne za svaki prozor — inače su debug nizovi uvijek od zadnjeg
 * prozora, ne od onog koji je trigerirao detekciju. */
void GCC_SnapshotRaw(const uint16_t *buf, uint32_t frame_offset)
{
    const uint16_t *p = &buf[frame_offset * NUM_CH];
    for (int s = 0; s < FFT_SIZE; s++) {
        dbg_raw_ch0[s] = p[s * NUM_CH + 0];
        dbg_raw_ch1[s] = p[s * NUM_CH + 1];
        dbg_raw_ch2[s] = p[s * NUM_CH + 2];
        dbg_raw_ch3[s] = p[s * NUM_CH + 3];
    }
    uint8_t tmp;
    tmp = 0;
    (void) tmp;
}

float GCC_RMS(const float *ch)
{
    float acc = 0.0f;
    for (int s = 0; s < FFT_SIZE; s++) {
        acc += ch[s] * ch[s];
    }
    return sqrtf(acc / (float)FFT_SIZE);
}

/*
 * CMSIS-DSP packing realnog FFT-a (N = FFT_SIZE):
 *   fft[0] = bin 0 (DC)        — realan
 *   fft[1] = bin N/2 (Nyquist) — realan, upakiran u DC imag slot
 *   fft[2k]   = bin k realni dio   (k = 1 .. N/2-1)
 *   fft[2k+1] = bin k imaginarni
 *
 * Cross-spektar conj(X) * Y, PHAT normalizacija dijeli s |conj(X)*Y|.
 */
void GCC_PHAT(const float *ref, const float *sig, float *corr)
{
    static float fft_x[FFT_SIZE];
    static float fft_y[FFT_SIZE];
    static float fft_c[FFT_SIZE];

    arm_rfft_fast_f32(&s_rfft, (float32_t *)ref, fft_x, 0);
    arm_rfft_fast_f32(&s_rfft, (float32_t *)sig, fft_y, 0);

    /* Bin 0 (DC) — realan */
    {
        float cre = fft_x[0] * fft_y[0];
        float mag = fabsf(cre);
        float w   = (mag > 1e-9f) ? (1.0f / (mag + 1e-9f)) : 0.0f;
        fft_c[0]  = cre * w;
    }

    /* Bin N/2 (Nyquist) — realan, u slotu fft[1] */
    {
        float cre = fft_x[1] * fft_y[1];
        float mag = fabsf(cre);
        float w   = (mag > 1e-9f) ? (1.0f / (mag + 1e-9f)) : 0.0f;
        fft_c[1]  = cre * w;
    }

    /* Binovi 1 … N/2-1 — kompleksni */
    for (int k = 1; k < FFT_SIZE / 2; k++) {
        int ire = 2 * k;
        int iim = 2 * k + 1;

        float re1 = fft_x[ire], im1 = fft_x[iim];
        float re2 = fft_y[ire], im2 = fft_y[iim];

        /* conj(X) * Y — daje peak na POZITIVNOM lagu kad sig kasni za ref,
         * što se poklapa s konvencijom u headeru i s NCC_FindTDOA.
         * (X*conj(Y) bi dao peak na −lag → zrcaljen/okrenut azimut.) */
        float cre = re1 * re2 + im1 * im2;
        float cim = re1 * im2 - im1 * re2;

        float mag = sqrtf(cre * cre + cim * cim);
        float w   = (mag > 1e-9f) ? (1.0f / (mag + 1e-9f)) : 0.0f;

        fft_c[ire] = cre * w;
        fft_c[iim] = cim * w;
    }

    /* Inverzni RFFT → realna korelacijska sekvenca */
    arm_rfft_fast_f32(&s_rfft, fft_c, corr, 1);
}

float GCC_FindTDOA(const float *corr, float *qual_out)
{
    /* Maksimum samo unutar fizički mogućeg raspona lagova.
     * Za 10 cm brid pri 64 kHz: max ≈ 18.66 uzoraka → TDOA_MAX_SAMPLES = 20. */
    const int max_lag = TDOA_MAX_SAMPLES;

    /* Traži apsolutni peak — hvata i negativne pikove (invertiran polaritet kanala). */
    int   max_idx  = 0;
    float best_abs = fabsf(corr[0]);

    /* Pozitivni lagovi: 1 … max_lag */
    for (int i = 1; i <= max_lag; i++) {
        float a = fabsf(corr[i]);
        if (a > best_abs) { best_abs = a; max_idx = i; }
    }
    /* Negativni lagovi: cirkularno na kraju buffera (FFT_SIZE-max_lag … FFT_SIZE-1) */
    for (int i = FFT_SIZE - max_lag; i < FFT_SIZE; i++) {
        float a = fabsf(corr[i]);
        if (a > best_abs) { best_abs = a; max_idx = i; }
    }

    /* Quality: omjer apsolutnog pika i srednje apsolutne vrijednosti cijele korelacije. */
    if (qual_out) {
        float sum = 0.0f;
        for (int i = 0; i < FFT_SIZE; i++) sum += fabsf(corr[i]);
        float mean_abs = sum / (float)FFT_SIZE;
        *qual_out = (mean_abs > 1e-9f) ? (best_abs / mean_abs) : 0.0f;
    }

    /* Parabolička interpolacija s cirkularnim susjedima — radi na APSOLUTNIM
     * vrijednostima da uvijek interpolira prema vrhu, neovisno o predznaku.
     * Inače za negativan (invertiran) pik parabola se okrene naopako i delta
     * pokazuje od vrha → sub-sample pogreška do ±0.5 uzorka u TDOA. */
    int   il = (max_idx == 0)            ? (FFT_SIZE - 1) : (max_idx - 1);
    int   ir = (max_idx == FFT_SIZE - 1) ? 0              : (max_idx + 1);
    float c0 = fabsf(corr[il]);
    float c1 = best_abs;
    float c2 = fabsf(corr[ir]);

    float denom = (c0 - 2.0f * c1 + c2);
    float delta = 0.0f;
    if (fabsf(denom) > 1e-12f) {
        delta = 0.5f * (c0 - c2) / denom;
        if (delta > 1.0f)  delta = 1.0f;
        if (delta < -1.0f) delta = -1.0f;
    }

    float frac = (float)max_idx + delta;
    if (frac < 0.0f)              frac += (float)FFT_SIZE;
    else if (frac >= (float)FFT_SIZE) frac -= (float)FFT_SIZE;

    /* Cirkularno: indeksi < N/2 = pozitivni lagovi, ≥ N/2 = negativni */
    float delay_samp = (frac < (float)(FFT_SIZE / 2))
                       ? frac
                       : frac - (float)FFT_SIZE;

    return delay_samp * SAMPLE_PERIOD_S;
}
