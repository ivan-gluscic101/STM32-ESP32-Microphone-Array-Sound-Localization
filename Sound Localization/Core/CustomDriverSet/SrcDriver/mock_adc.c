#include "mock_adc.h"

/* Cijeli modul se kompajlira samo uz USE_MOCK_ADC (audio_common.h). Inače je
 * prazan — da ~68 KB mock tablica (s_mock_table + s_burst_source) ne troši BSS
 * kad se koriste pravi mikrofoni. */
#if USE_MOCK_ADC

#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159f
#endif

/* Period između pljeskova izražen u DMA half-events (1 event = 16 ms @ 64 kHz).
 * Mora biti > SILENCE_FRAMES_REQUIRED (12) inače HOLDOFF nikad ne pušta novi
 * pljesak. 24 evenata = 384 ms tišine između pljeskova. */
#define MOCK_EVENTS_PER_BURST 24

/* peakove na vlasnitim lag-ovima. Bijeli šum kao carrier eliminira to. */
#define BURST_CENTER_T        0.003f      /* sredina pljeska na ~3 ms unutar 16 ms half-buffera */
#define BURST_SIGMA           0.0008f     /* 0.8 ms — kratki transient */
#define BURST_AMPLITUDE       1500.0f     /* LSB peak */
#define DC_OFFSET             2048.0f

/* Pre-generirani izvor signala u koord. sustavu pljeska (t=0 u centru).
 * Svi mikrofoni vide ISTI signal s različitim delay-em — to je akustički
 * ispravno (one source, four observers) i daje clean cross-korelaciju. */
#define BURST_SOURCE_LEN      1024
#define BURST_SOURCE_CENTER   512
static float s_burst_source[BURST_SOURCE_LEN];

/* Pretkalkulirani burstovi — 8 × 4096 B = 32 KB BSS. */
static uint16_t s_mock_table[MOCK_NUM_DIRS][HALF_BUFFER];

/* Brojač eventa — wraparound svakih MOCK_NUM_DIRS × MOCK_EVENTS_PER_BURST. */
static uint32_t s_event_count = 0;

/* Debug eksportovi — vidljivi u debuggeru kao globalne varijable. */
volatile uint32_t g_mock_dir_idx     = 0xFFFFFFFFu;
volatile uint32_t g_mock_event_count = 0;

/* Linear congruential generator (rand() bi povukao newlib reentrant). */
static uint32_t s_rng = 0xDEADBEEFu;
static inline uint32_t mock_rand(void)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return s_rng;
}

/* Smjerovi prema izvoru (jedinični vektori). Pokriva 8 reprezentativnih
 * pravaca: ±X, ±Y, +Z, i tri diagonale. Očekivani kutovi nakon LOC3D:
 *
 *   index | (x,y,z)              | az (°)   | el (°)
 *   ──────┼──────────────────────┼──────────┼────────
 *      0  | ( 1, 0, 0)           |    0     |    0
 *      1  | ( 0, 1, 0)           |   90     |    0
 *      2  | (-1, 0, 0)           |  180     |    0
 *      3  | ( 0,-1, 0)           |  -90     |    0
 *      4  | ( 0, 0, 1)           |    *     |   90    (azimut nedefiniran)
 *      5  | ( 0.707, 0.707, 0)   |   45     |    0
 *      6  | ( 0.707, 0, 0.707)   |    0     |   45
 *      7  | ( 0, 0.707, 0.707)   |   90     |   45
 */
static const float DIRECTIONS[MOCK_NUM_DIRS][3] = {
    {  1.0000f,  0.0000f,  0.0000f },
    {  0.0000f,  1.0000f,  0.0000f },
    { -1.0000f,  0.0000f,  0.0000f },
    {  0.0000f, -1.0000f,  0.0000f },
    {  0.0000f,  0.0000f,  1.0000f },
    {  0.7071f,  0.7071f,  0.0000f },
    {  0.7071f,  0.0000f,  0.7071f },
    {  0.0000f,  0.7071f,  0.7071f },
};

/* Pozicije mikrofona u istom koord. sustavu kao audio_common.h. */
static const float MIC_X[NUM_CH] = { MIC1_X, MIC2_X, MIC3_X, MIC4_X };
static const float MIC_Y[NUM_CH] = { MIC1_Y, MIC2_Y, MIC3_Y, MIC4_Y };
static const float MIC_Z[NUM_CH] = { MIC1_Z, MIC2_Z, MIC3_Z, MIC4_Z };

/* Generira izvor pljeska — Gaussov envelope × bijeli šum, centriran u
 * BURST_SOURCE_CENTER. Pozvati jednom u Mock_Init prije svakog burst-a. */
static void mock_init_source(void)
{
    for (int i = 0; i < BURST_SOURCE_LEN; i++) {
        float t = (float)(i - BURST_SOURCE_CENTER) * SAMPLE_PERIOD_S;
        float env = expf(-(t * t) / (BURST_SIGMA * BURST_SIGMA));
        /* Bijeli šum [-1, 1] iz LCG-a — širok spektar je ključ za PHAT. */
        int32_t r = (int32_t)(mock_rand() >> 8) & 0xFFFF;
        float noise = (float)(r - 32768) / 32768.0f;
        s_burst_source[i] = BURST_AMPLITUDE * env * noise;
    }
}

/* Dohvaća vrijednost izvora na proizvoljno vrijeme t (sekunde, t=0 = centar).
 * Linearna interpolacija između susjednih sampleova. */
static inline float burst_sample_at(float t)
{
    float idx_f = t / SAMPLE_PERIOD_S + (float)BURST_SOURCE_CENTER;
    int idx_i = (int)idx_f;
    if (idx_i < 0 || idx_i >= BURST_SOURCE_LEN - 1) return 0.0f;
    float frac = idx_f - (float)idx_i;
    return s_burst_source[idx_i] * (1.0f - frac) + s_burst_source[idx_i + 1] * frac;
}

/* Generira pljesak iz zadanog smjera u dst (HALF_BUFFER samples, interleaved). */
static void mock_generate_burst(const float dir[3], uint16_t *dst)
{
    /* Akustički delay po mikrofonu:
     *   Val koji dolazi iz smjera +dir prvo pogađa mikrofon najviše u smjeru +dir.
     *   tau_k = -(dir · M_k) / c   (pozitivno = mikrofon je dalji od izvora)
     * Normaliziramo tako da min tau = 0 — pljesak započinje u t=0. */
    float tau_acoustic[NUM_CH];
    float tau_min = 0.0f;
    for (int k = 0; k < NUM_CH; k++) {
        tau_acoustic[k] = -(dir[0] * MIC_X[k] + dir[1] * MIC_Y[k] + dir[2] * MIC_Z[k])
                          / SPEED_OF_SOUND;
        if (k == 0 || tau_acoustic[k] < tau_min) tau_min = tau_acoustic[k];
    }
    for (int k = 0; k < NUM_CH; k++) tau_acoustic[k] -= tau_min;

    for (uint32_t s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        for (uint32_t k = 0; k < NUM_CH; k++) {
            /* Trenutak kad ADC fizički uzima ovaj sample za mikrofon k:
             *   t_sample = s · Ts + k · CH_DELAY_S */
            float t_sample = (float)s * SAMPLE_PERIOD_S + (float)k * CH_DELAY_S;

            /* Vrijeme u koord. izvora — t=0 je centar burst-a u svim mikrofonima.
             * Mikrofon k vidi burst sa zakašnjenjem tau_acoustic[k] + BURST_CENTER_T. */
            float t_src = t_sample - tau_acoustic[k] - BURST_CENTER_T;
            float signal = burst_sample_at(t_src);

            /* ADC šumni pod ±16 LSB */
            int32_t noise = (int32_t)(mock_rand() & 0x1Fu) - 16;

            int32_t v = (int32_t)(DC_OFFSET + signal) + noise;
            if (v < 0)    v = 0;
            if (v > 4095) v = 4095;
            dst[s * NUM_CH + k] = (uint16_t)v;
        }
    }
}

void Mock_Init(void)
{
    s_event_count = 0;
    s_rng = 0xDEADBEEFu;
    mock_init_source();   /* generira jedan zajednički izvor (bijeli šum × envelope) */
    for (int d = 0; d < MOCK_NUM_DIRS; d++) {
        mock_generate_burst(DIRECTIONS[d], s_mock_table[d]);
    }
}

void Mock_FillHalf(uint16_t *dst)
{
    uint32_t phase = s_event_count % MOCK_EVENTS_PER_BURST;

    if (phase == 0) {
        uint32_t dir_idx = (s_event_count / MOCK_EVENTS_PER_BURST) % MOCK_NUM_DIRS;
        g_mock_dir_idx = dir_idx;   /* debug: označi koji smjer ide u ovaj buffer */
        memcpy(dst, s_mock_table[dir_idx], HALF_BUFFER * sizeof(uint16_t));
    } else {
        /* Tišina + ADC šumni pod */
        for (uint32_t i = 0; i < HALF_BUFFER; i++) {
            int32_t noise = (int32_t)(mock_rand() & 0x1Fu) - 16;
            int32_t v = (int32_t)DC_OFFSET + noise;
            dst[i] = (uint16_t)v;
        }
    }
    s_event_count++;
    g_mock_event_count = s_event_count;
}

#endif /* USE_MOCK_ADC */
