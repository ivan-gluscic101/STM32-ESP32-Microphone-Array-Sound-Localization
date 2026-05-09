/*
 * TwoDimSoundLoc.c
 *
 * Threshold-based TDOA lokalizacija zvuka — 2 mikrofona, 1D kut.
 *
 * Raspored kanala u interleaved bufferu:
 *   buf[s*4 + 0] = CH1 (mic1, PA0,  RANK1) — uzorkovan u t = s*62.5µs
 *   buf[s*4 + 1] = CH2 (mic2, PB14, RANK2) — uzorkovan u t = s*62.5µs + 0.9µs
 *   buf[s*4 + 2] = CH3 (mic3, PC0,  RANK3) — nije korišten
 *   buf[s*4 + 3] = CH4 (mic4, PC1,  RANK4) — nije korišten
 *
 * TDOA formula:
 *   TDOA = (n1 - n2) * SAMPLE_PERIOD_S - CH_DELAY_S
 *
 * Kut formula (far-field aproksimacija):
 *   φ = arcsin(TDOA * c / d)   →   φ ∈ [-90°, +90°]
 *
 * ISPRAVKE:
 *   1. HALF_SIZE sada dolazi iz audio_common.h i iznosi 512 (bio je 256).
 *      LOC_Process sada skenira CIJELI half-buffer umjesto samo prvih 256
 *      uzoraka — dvostruko veća šansa za detekciju zvučnog eventa.
 */

#include "TwoDimSoundLoc.h"

uint8_t LOC_Process(const uint16_t *buf, int16_t *phi_tenth_deg, uint8_t *strength)
{
    static uint8_t  state          = 0;  /* 0 = IDLE, 1 = ACTIVE (cooldown) */
    static uint8_t  silence_frames = 0;

    int32_t  n1 = -1, n2 = -1;
    uint16_t ch0, ch1;
    uint16_t amplitude_max = 0;   /* max ch0 vrijednost (za pozitivni vrh) */
    uint16_t amplitude_min = 4095;/* min ch0 vrijednost (za negativni vrh) */

    /* ── Cooldown PRVO — ako smo u ACTIVE, samo broji i izađi ────────────── */
    if (state == 1)
    {
        if (++silence_frames >= SILENCE_FRAMES)
        {
            state          = 0;
            silence_frames = 0;
        }
        return 0;
    }

    /* ── Threshold scan po cijelom half-bufferu (NE break-amo rano da   */
    /*    ispravno izračunamo amplitude vrhova) ─────────────────────── */
    for (int s = 0; s < HALF_SIZE; s++)
    {
        ch0 = buf[s * NUM_CH + 0];   /* mic1 — PA0  */
        ch1 = buf[s * NUM_CH + 1];   /* mic2 — PB14 */

        if (n1 < 0 && (ch0 > THRESHOLD_HIGH || ch0 < THRESHOLD_LOW)) n1 = s;
        if (n2 < 0 && (ch1 > THRESHOLD_HIGH || ch1 < THRESHOLD_LOW)) n2 = s;

        if (ch0 > amplitude_max) amplitude_max = ch0;
        if (ch0 < amplitude_min) amplitude_min = ch0;
    }

    /* ── IDLE — traži novi zvučni događaj ───────────────────────────────── */
    if (n1 < 0 || n2 < 0) return 0;

    /* ── Sanity check — fizikalni maksimum ──────────────────────────────── */
    int32_t delta_n = n1 - n2;
    if (delta_n > TDOA_MAX_SAMPLES || delta_n < -TDOA_MAX_SAMPLES) return 0;

    /* ── Strength iz max ABS odstupanja od idle baseline-a ~3070 ─────────── */
    /* Pozitivni vrh: amplitude_max - THRESHOLD_HIGH                          */
    /* Negativni vrh: THRESHOLD_LOW - amplitude_min  (mjereno od LOW praga,   */
    /* pa "1" znači jedva preko praga, "1500" potpuna saturacija)             */
    int32_t pos_dev = (int32_t)amplitude_max - THRESHOLD_HIGH;
    int32_t neg_dev = (int32_t)THRESHOLD_LOW - (int32_t)amplitude_min;
    int32_t peak_dev = (pos_dev > neg_dev) ? pos_dev : neg_dev;

    int32_t str = peak_dev * 100 / (4095 - THRESHOLD_HIGH);
    if (str < 1)   str = 1;     /* trigger je već prošao prag, minimalni signal je 1 */
    if (str > 100) str = 100;
    *strength = (uint8_t)str;

    /* ── Tek SAD ulazimo u cooldown (nakon što znamo da je signal valjan).   */
    /* Time spurious slabi triggeri ne blokiraju pravi pljesak na 64 ms.     */
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

    return 1;
}