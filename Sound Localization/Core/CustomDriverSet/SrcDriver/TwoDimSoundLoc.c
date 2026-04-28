/*
 * TwoDimSoundLoc.c
 * Implementirano: 2026-04-02
 *
 * Threshold-based TDOA lokalizacija zvuka — 2 mikrofona, 1D kut.
 *
 * Raspored kanala u interleaved bufferu:
 *   buf[s*4 + 0] = CH1 (mic1, PA0,  RANK1) — uzorkovan u t = s*62.5µs
 *   buf[s*4 + 1] = CH2 (mic2, PB14, RANK2) — uzorkovan u t = s*62.5µs + 0.9µs
 *   buf[s*4 + 2] = CH3 (mic3, PC0,  RANK3) — nije korišten u ovom algoritmu
 *   buf[s*4 + 3] = CH4 (mic4, PC1,  RANK4) — nije korišten u ovom algoritmu
 *
 * TDOA formula:
 *   TDOA = (n1 - n2) * SAMPLE_PERIOD_S - CH_DELAY_S
 *
 * Kut formula (far-field aproksimacija):
 *   φ = arcsin(TDOA * c / d)   →   φ ∈ [-90°, +90°]
 *
 *   φ = 0°   → zvuk dolazi okomito na os mikrofona
 *   φ = +90° → zvuk dolazi direktno s mic2 strane
 *   φ = -90° → zvuk dolazi direktno s mic1 strane
 */

#include "TwoDimSoundLoc.h"

uint8_t LOC_Process(const uint16_t *buf, int16_t *phi_tenth_deg, uint8_t *strength)
{
    static uint8_t  state          = 0;  /* 0 = IDLE, 1 = ACTIVE */
    static uint8_t  silence_frames = 0;

    int32_t  n1 = -1, n2 = -1;
    uint16_t ch0, ch1;
    uint16_t amplitude = 0;

    /* ── Korak 1: threshold scan ─────────────────────────────────────────── */
    for (int s = 0; s < HALF_SIZE; s++)
    {
        ch0 = buf[s * NUM_CH + 0];   /* mic1 — PA0  */
        ch1 = buf[s * NUM_CH + 1];   /* mic2 — PB14 */

        if (n1 < 0 && (ch0 > THRESHOLD_HIGH || ch0 < THRESHOLD_LOW)) n1 = s;
        if (n2 < 0 && (ch1 > THRESHOLD_HIGH || ch1 < THRESHOLD_LOW)) n2 = s;
        if (ch0 > amplitude) amplitude = ch0;
        if (n1 >= 0 && n2 >= 0) break;
    }

    /* ── State machine — fiksni cooldown ────────────────────────────────── */
       if (state == 1)  /* ACTIVE — broji sve frame-ove, bez reseta */
       {
           if (++silence_frames >= SILENCE_FRAMES)
           {
               state          = 0;
               silence_frames = 0;
           }
           return 0;
       }

    /* ── IDLE — traži novi zvučni događaj ───────────────────────────────── */
    if (n1 < 0 || n2 < 0) return 0;

    /* ── Sanity check — fizikalni maksimum ──────────────────────────────── */
    int32_t delta_n = n1 - n2;
    if (delta_n > TDOA_MAX_SAMPLES || delta_n < -TDOA_MAX_SAMPLES) return 0;

    /* Prešli smo u ACTIVE */
    state          = 1;
    silence_frames = 0;

    /* ── TDOA u sekundama (s korekcijom ADC channel offseta) ────────────── */
    float tdoa = (float)delta_n * SAMPLE_PERIOD_S - CH_DELAY_S;

    /* ── Normalizirani omjer za arcsin, stegnut na [-1.0, 1.0] ──────────── */
    float ratio = tdoa * SPEED_OF_SOUND / MIC_DIST_M;
    if (ratio >  1.0f) ratio =  1.0f;
    if (ratio < -1.0f) ratio = -1.0f;

    /* ── Kut u stupnjevima ───────────────────────────────────────────────── */
    float phi_deg = asinf(ratio) * (180.0f / 3.14159265f);
    *phi_tenth_deg = (int16_t)(phi_deg * 10.0f);

    /* ── Signal strength 0-100 ───────────────────────────────────────────── */
    int32_t str = ((int32_t)amplitude - THRESHOLD_HIGH) * 100 / (4095 - THRESHOLD_HIGH);
    if (str < 0)   str = 0;
    if (str > 100) str = 100;
    *strength = (uint8_t)str;

    return 1;  /* valjan rezultat */
}
