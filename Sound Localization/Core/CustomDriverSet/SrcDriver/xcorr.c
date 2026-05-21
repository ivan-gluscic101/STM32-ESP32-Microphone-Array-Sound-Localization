#include "xcorr.h"
#include "arm_math.h"
#include <math.h>
#include <string.h>

/* FFT duljina = 2 × SAMPLES_PER_CHANNEL = 1024 (zero-padding eliminira kružnu aliasing) */
#define GCC_FFT_LEN  (2 * SAMPLES_PER_CHANNEL)

/* ── Statički bufferi (BSS, ne na stacku) ─────────────────────────────────── */
/* s_fft_in   [4 KB] — scratch: windowed+zero-padded ulaz, reuse kao IFFT izlaz */
/* s_fft_ref  [4 KB] — RFFT referentnog kanala (cachiran između poziva)          */
/* s_fft_sig  [4 KB] — RFFT trenutnog sig kanala                                 */
/* s_cross    [4 KB] — PHAT cross-spectrum → IFFT input                         */
/* s_hann     [2 KB] — preračunati Hann prozor                                  */
/* Ukupno: ~18 KB u BSS (STM32G474 ima 128 KB SRAM)                            */
static arm_rfft_fast_instance_f32 s_rfft;
static float s_fft_in [GCC_FFT_LEN];
static float s_fft_ref[GCC_FFT_LEN];
static float s_fft_sig[GCC_FFT_LEN];
static float s_cross  [GCC_FFT_LEN];
static float s_hann   [SAMPLES_PER_CHANNEL];

/* ── Inicijalizacija (jednom pri startu) ─────────────────────────────────── */
void XCorr_Init(void)
{
    arm_rfft_fast_init_f32(&s_rfft, GCC_FFT_LEN);

    for (int n = 0; n < SAMPLES_PER_CHANNEL; n++) {
        s_hann[n] = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * n
                                          / (SAMPLES_PER_CHANNEL - 1)));
    }
}

/* ── Deinterleave + DC removal ───────────────────────────────────────────── */
void XCorr_ExtractChannels(const uint16_t *buf,
                           float ch0[], float ch1[],
                           float ch2[], float ch3[])
{
    float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f;
    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        sum0 += (float)buf[s * NUM_CH + 0];
        sum1 += (float)buf[s * NUM_CH + 1];
        sum2 += (float)buf[s * NUM_CH + 2];
        sum3 += (float)buf[s * NUM_CH + 3];
    }
    float inv_n = 1.0f / SAMPLES_PER_CHANNEL;
    float dc0 = sum0 * inv_n, dc1 = sum1 * inv_n;
    float dc2 = sum2 * inv_n, dc3 = sum3 * inv_n;

    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        ch0[s] = (float)buf[s * NUM_CH + 0] - dc0;
        ch1[s] = (float)buf[s * NUM_CH + 1] - dc1;
        ch2[s] = (float)buf[s * NUM_CH + 2] - dc2;
        ch3[s] = (float)buf[s * NUM_CH + 3] - dc3;
    }
}

/* ── RMS ─────────────────────────────────────────────────────────────────── */
float XCorr_RMS(const float *ch)
{
    float acc = 0.0f;
    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) acc += ch[s] * ch[s];
    return sqrtf(acc / (float)SAMPLES_PER_CHANNEL);
}

/* ── FFT referentnog kanala (cachira se za sva 3 para u frameu) ──────────── */
void XCorr_PrepareRef(const float *ref)
{
    for (int n = 0; n < SAMPLES_PER_CHANNEL; n++) {
        s_fft_in[n] = ref[n] * s_hann[n];
    }
    memset(s_fft_in + SAMPLES_PER_CHANNEL, 0,
           SAMPLES_PER_CHANNEL * sizeof(float));

    arm_rfft_fast_f32(&s_rfft, s_fft_in, s_fft_ref, 0);
}

/* ── GCC-PHAT lag ────────────────────────────────────────────────────────── */
/*
 * arm_rfft_fast_f32 packed output za N=1024:
 *   [0]      = DC  (real only)
 *   [1]      = Nyquist (real only)
 *   [2k],[2k+1] = Re/Im bin k, k=1..511
 *
 * PHAT: G_phat[k] = conj(X[k]) · Y[k] / |conj(X[k]) · Y[k]|
 *   → svaki bin postaje jedinični fazor, IFFT daje oštri peak na pravom lagu.
 *
 * Lag mapping u IFFT izlazu duljine 1024:
 *   out[0]       = lag  0
 *   out[l]       = lag +l,  l = 1..511
 *   out[1024-l]  = lag -l,  l = 1..511
 */
int32_t XCorr_ComputeLag(const float *sig)
{
    /* Window + zero-pad signal */
    for (int n = 0; n < SAMPLES_PER_CHANNEL; n++) {
        s_fft_in[n] = sig[n] * s_hann[n];
    }
    memset(s_fft_in + SAMPLES_PER_CHANNEL, 0,
           SAMPLES_PER_CHANNEL * sizeof(float));

    arm_rfft_fast_f32(&s_rfft, s_fft_in, s_fft_sig, 0);

    /* ── PHAT kros-spektar ────────────────────────────────────────────────── */

    /* DC bin — oba realna, samo predznak */
    {
        float g = s_fft_ref[0] * s_fft_sig[0];
        s_cross[0] = (g > 0.0f) ? 1.0f : (g < 0.0f) ? -1.0f : 0.0f;
    }
    /* Nyquist bin — isto realan */
    {
        float g = s_fft_ref[1] * s_fft_sig[1];
        s_cross[1] = (g > 0.0f) ? 1.0f : (g < 0.0f) ? -1.0f : 0.0f;
    }
    /* Kompleksni binovi k = 1..511 */
    for (int k = 1; k < GCC_FFT_LEN / 2; k++) {
        float xr = s_fft_ref[2*k],   xi = s_fft_ref[2*k + 1];
        float yr = s_fft_sig[2*k],   yi = s_fft_sig[2*k + 1];

        /* G = conj(X) · Y */
        float gr = xr * yr + xi * yi;
        float gi = xr * yi - xi * yr;

        /* PHAT normalizacija */
        float mag = sqrtf(gr * gr + gi * gi);
        if (mag > 1e-9f) { s_cross[2*k] = gr / mag; s_cross[2*k+1] = gi / mag; }
        else             { s_cross[2*k] = 0.0f;      s_cross[2*k+1] = 0.0f; }
    }

    /* ── IFFT → time-domain GCC-PHAT korelacija ──────────────────────────── */
    /* Reuse s_fft_in kao output (s_cross i s_fft_in ne preklapaju). */
    arm_rfft_fast_f32(&s_rfft, s_cross, s_fft_in, 1);

    /* ── Peak search u ±TDOA_MAX_SAMPLES ─────────────────────────────────── */
    float   best_val = s_fft_in[0];
    int32_t best_lag = 0;

    for (int32_t l = 1; l <= TDOA_MAX_SAMPLES; l++) {
        /* lag = +l */
        if (s_fft_in[l] > best_val) {
            best_val = s_fft_in[l];
            best_lag = l;
        }
        /* lag = -l (pohranjen na kraju IFFT buffera) */
        if (s_fft_in[GCC_FFT_LEN - l] > best_val) {
            best_val = s_fft_in[GCC_FFT_LEN - l];
            best_lag = -l;
        }
    }

    return best_lag;
}
