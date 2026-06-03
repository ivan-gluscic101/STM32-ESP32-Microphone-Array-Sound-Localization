#include "sound_loc_2d.h"
#include "gcc_phat.h"
#include "ncc_tdoa.h"
#include <math.h>

/* ── Geometrija ─────────────────────────────────────────────────────────────
 * Jednakostraničan trokut, brid d = 0.10 m.
 *   M1 = (0,       0    )   RANK1, PB14
 *   M2 = (0.10,    0    )   RANK2, PC0
 *   M3 = (0.05,    0.0866)  RANK3, PC1
 *
 * Preračunati inverzi za brzo atan2 računanje:
 *   ux = τ12 / M2x
 *   uy = (τ13 - 0.5 × τ12) / M3y
 *   θ  = atan2(uy, ux)
 * ─────────────────────────────────────────────────────────────────────────── */
#define M2X         0.10f
#define M3X         0.05f
#define M3Y         0.08660254038f   /* 0.10 × √3/2 */

#define INV_M2X     (1.0f / M2X)             /* 10.0 */
#define M3X_M2X     (M3X / M2X)              /* 0.5  */
#define INV_M3Y     (1.0f / M3Y)             /* 11.547 */

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* ── Pragovi ────────────────────────────────────────────────────────────────
 * Isti principi kao u sound_loc_3d.c — podesi prema signalu. */
#define MIN_RMS_THRESHOLD       15.0f
#define MIN_RMS_PER_CHANNEL     (MIN_RMS_THRESHOLD * 0.3f)
#define NCC_MIN_QUALITY         2.0f

/* Cooldown: 1 poziv ≈ 16 ms (jedan half-buffer) → 19 ≈ 300 ms */
#define DETECTION_COOLDOWN_FRAMES  19

static uint16_t s_cooldown = 0;

/* ── Kanalni nizovi (statički — 3 × 1024 × 4 B = 12 KB u BSS-u) ─────────── */
static float s_ch0[SAMPLES_PER_CHANNEL];
static float s_ch1[SAMPLES_PER_CHANNEL];
static float s_ch2[SAMPLES_PER_CHANNEL];

/* ── Energetski peak finder ─────────────────────────────────────────────────
 * Inkrementalni sliding-window energy scan (isti algoritam kao u 3D).
 * Vraća frame_offset koji centrira GCC prozor na energetski vrh. */
static uint32_t find_peak_offset(const uint16_t *buf)
{
    const uint32_t TOTAL = 2u * SAMPLES_PER_CHANNEL;
    const uint32_t PROBE = 16u;
    const uint32_t HALF  = SAMPLES_PER_CHANNEL / 2u;

    uint32_t win_e = 0u;
    for (uint32_t i = 0u; i < PROBE; i++) {
        const uint16_t *s = &buf[i * NUM_CH];
        for (uint32_t ch = 0u; ch < 3u; ch++) {   /* samo M1/M2/M3 */
            int32_t v = (int32_t)s[ch] - 2048;
            win_e += (uint32_t)((uint32_t)(v * v) >> 6);
        }
    }
    uint32_t best_e     = win_e;
    uint32_t best_frame = PROBE / 2u;

    for (uint32_t f = 1u; f + PROBE <= TOTAL; f++) {
        for (uint32_t ch = 0u; ch < 3u; ch++) {
            int32_t v_old = (int32_t)buf[(f - 1u) * NUM_CH + ch] - 2048;
            int32_t v_new = (int32_t)buf[(f + PROBE - 1u) * NUM_CH + ch] - 2048;
            win_e -= (uint32_t)((uint32_t)(v_old * v_old) >> 6);
            win_e += (uint32_t)((uint32_t)(v_new * v_new) >> 6);
        }
        if (win_e > best_e) {
            best_e     = win_e;
            best_frame = f + PROBE / 2u;
        }
    }

    if (best_frame < HALF)         return 0u;
    if (best_frame + HALF > TOTAL) return TOTAL - SAMPLES_PER_CHANNEL;
    return best_frame - HALF;
}

/* ── Glavna funkcija ───────────────────────────────────────────────────────── */
uint8_t LOC2D_Process(const uint16_t *buf, loc3d_result_t *out)
{
    if (s_cooldown > 0) { s_cooldown--; return 0; }

    /* 1. Pronađi energetski vrh i centriraj prozor */
    uint32_t offset = find_peak_offset(buf);

    /* 2. Ekstrakcija kanala: DC removal + Hann window
     *    GCC_ExtractChannels deinterleava sva 4 kanala — 4. se ne koristi. */
    static float dummy[SAMPLES_PER_CHANNEL];
    GCC_ExtractChannels(buf, offset, s_ch0, s_ch1, s_ch2, dummy);

    /* 3. RMS gate — samo M1/M2/M3 */
    float rms0 = GCC_RMS(s_ch0);
    float rms1 = GCC_RMS(s_ch1);
    float rms2 = GCC_RMS(s_ch2);

    float rms_max = rms0;
    if (rms1 > rms_max) rms_max = rms1;
    if (rms2 > rms_max) rms_max = rms2;

    float rms_min = rms0;
    if (rms1 < rms_min) rms_min = rms1;
    if (rms2 < rms_min) rms_min = rms2;

    if (rms_max < MIN_RMS_THRESHOLD)   return 0;
    if (rms_min < MIN_RMS_PER_CHANNEL) return 0;

    /* 4. Time-domain TDOA: M1 kao referenca */
    float qual12, qual13;
    float tau12_meas = NCC_FindTDOA(s_ch0, s_ch1, &qual12);
    float tau13_meas = NCC_FindTDOA(s_ch0, s_ch2, &qual13);

    if (qual12 < NCC_MIN_QUALITY || qual13 < NCC_MIN_QUALITY) return 0;

    /* 5. Korekcija ADC sekvencijalnog offseta
     *    M2 = RANK2 → +1 × CH_DELAY_S, M3 = RANK3 → +2 × CH_DELAY_S */
    float tau12 = tau12_meas + 1.0f * CH_DELAY_S;
    float tau13 = tau13_meas + 2.0f * CH_DELAY_S;

    /* 6. Smjer (far-field plane wave):
     *    ux = τ12 / M2x
     *    uy = (τ13 − (M3x/M2x) × τ12) / M3y */
    float ux = tau12 * INV_M2X;
    float uy = (tau13 - M3X_M2X * tau12) * INV_M3Y;

    /* 7. Azimutni kut */
    float az_rad = atan2f(uy, ux);

    out->az_tenth = (int16_t)(az_rad * (1800.0f / PI));
    out->el_tenth = 0;

    /* 8. Jakost (isti dB-like mapping kao u 3D) */
    float str_f = 20.0f * log10f(rms_max + 1.0f);
    if (str_f <   1.0f) str_f =   1.0f;
    if (str_f > 100.0f) str_f = 100.0f;
    out->strength = (uint8_t)str_f;

    s_cooldown = DETECTION_COOLDOWN_FRAMES;
    return 1;
}
