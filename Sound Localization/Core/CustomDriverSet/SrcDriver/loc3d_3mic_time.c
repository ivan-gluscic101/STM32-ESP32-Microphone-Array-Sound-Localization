#include "loc3d_3mic_time.h"
#include "raw_capture.h"   /* RawCapture_Snapshot (memcpy raw uzoraka za UART debug) */
#include <math.h>

/*
 * loc3d_3mic_time.c — time-domain TDOA lokalizacija (M1, M2, M3) PRAGOM.
 *
 * Princip (bez FFT, bez micanja DC-a, bez energy peak finda):
 *   - DC je fiksno DC_LEVEL (2050). Uzorak "okida" kad |sample − DC| > THR (250).
 *   - Idemo redom kroz uzorke od početka buffera. Za svaki kanal (M1/M2/M3)
 *     bilježimo PRVI uzorak u kojem okine. ref_mic = onaj koji okine prvi.
 *   - Razlika onset indeksa između kanala = TDOA (u uzorcima). Iz toga smjer.
 *
 * TDOA se za geometriju uvijek formira relativno na M1 (tau12 = onset_M2 −
 * onset_M1, tau13 = onset_M3 − onset_M1), neovisno o tome koji je kanal fizički
 * okinuo prvi.
 *
 * Debug snapshot (dbgt_*) — čita se u debuggeru, ne ide na UART.
 */

volatile int32_t  dbgt_onset1, dbgt_onset2, dbgt_onset3;   /* onset indeksi (uzorci), -1 = nije okinuo */
volatile int32_t  dbgt_ref_mic;                            /* 0/1/2 = M1/M2/M3 okinuo prvi */
volatile float    dbgt_tau12_meas, dbgt_tau13_meas;        /* prije CH_DELAY korekcije */
volatile float    dbgt_tau12_corr, dbgt_tau13_corr;        /* poslije korekcije        */
volatile int32_t  dbgt_peak_abs[3];                         /* max |signal-DC| po kanalu */
volatile float    dbgt_sx, dbgt_sy, dbgt_sz;

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* Ukupan broj frejmova u sliding bufferu. */
#define WIN_FRAMES   (2u * SAMPLES_PER_CHANNEL)

/* Fiksna "DC" razina i prag odstupanja. Uzorak okida kad |sample − DC| > THR. */
#define DC_LEVEL         2050
#define THRESHOLD_LEVEL  250

/* Maksimalna razlika onseta između kanala (uzoraka). Veće → odbaci (nekonzistentno).
 * Najveći baseline = stranica trokuta; vidi TDOA_MAX_SAMPLES (+ rezerva). */
#define ONSET_MAX_SPREAD  (TDOA_MAX_SAMPLES + 6)

/* Cooldown nakon detekcije (broj poziva Process). Frame = half-buffer =
 * SAMPLES_PER_CHANNEL/fs = 1024/192kHz ≈ 5.33 ms. 45 × 5.33 ms ≈ 240 ms —
 * propusti rep pljeska i prve odjeke. (Skalira s fs: digni ako povisiš fs.) */
#define DETECTION_COOLDOWN_FRAMES  45

static uint16_t s_cooldown = 0;

/* Geometrija: M_geom2 = c·inv(D2) (D2 = baseline vektori M2−M1, M3−M1). */
static float M_geom2[2][2];

static void invert2x2(const float m[2][2], float inv[2][2])
{
    float det  = m[0][0] * m[1][1] - m[0][1] * m[1][0];
    float idet = (fabsf(det) > 1e-12f) ? (1.0f / det) : 0.0f;

    inv[0][0] =  m[1][1] * idet;
    inv[0][1] = -m[0][1] * idet;
    inv[1][0] = -m[1][0] * idet;
    inv[1][1] =  m[0][0] * idet;
}

void LOC3D_3MIC_TIME_Init(void)
{
    const float D2[2][2] = {
        { MIC2_X - MIC1_X, MIC2_Y - MIC1_Y },
        { MIC3_X - MIC1_X, MIC3_Y - MIC1_Y }
    };
    float D2inv[2][2];
    invert2x2(D2, D2inv);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            M_geom2[i][j] = SPEED_OF_SOUND * D2inv[i][j];
        }
    }
}

/*
 * Nađi onset jednog kanala: prvi uzorak u kojem |sample − DC_LEVEL| > THRESHOLD_LEVEL.
 * Vrati indeks uzorka (0..WIN_FRAMES-1) ili -1 ako prag nije prijeđen u prozoru.
 * Usput vrati maksimalno apsolutno odstupanje (za jakost / debug).
 */
static int32_t find_onset(const uint16_t *buf, uint32_t ch, int32_t *max_abs_out)
{
    int32_t onset   = -1;
    int32_t max_abs = 0;

    for (uint32_t s = 0u; s < WIN_FRAMES; s++) {
        int32_t dev = (int32_t)buf[s * NUM_CH + ch] - DC_LEVEL;
        if (dev < 0) dev = -dev;                 /* |sample − DC| */
        if (dev > max_abs) max_abs = dev;
        if (onset < 0 && dev > THRESHOLD_LEVEL) {
            onset = (int32_t)s;                  /* prvi prelazak praga */
        }
    }
    *max_abs_out = max_abs;
    return onset;
}

uint8_t LOC3D_3MIC_TIME_Process(const uint16_t *buf, loc3d_3mic_time_result_t *out)
{
    if (s_cooldown > 0) { s_cooldown--; return 0; }

    /* 1. Onset (prvi prelazak praga) i max odstupanje po kanalu M1/M2/M3. */
    int32_t max_abs[3];
    int32_t onset[3];
    onset[0] = find_onset(buf, 0u, &max_abs[0]);
    onset[1] = find_onset(buf, 1u, &max_abs[1]);
    onset[2] = find_onset(buf, 2u, &max_abs[2]);

    dbgt_onset1 = onset[0]; dbgt_onset2 = onset[1]; dbgt_onset3 = onset[2];
    dbgt_peak_abs[0] = max_abs[0]; dbgt_peak_abs[1] = max_abs[1]; dbgt_peak_abs[2] = max_abs[2];

    /* 2. Svi kanali moraju okinuti (preći prag) — inače nema valjane detekcije. */
    if (onset[0] < 0 || onset[1] < 0 || onset[2] < 0) return 0;

    /* ref_mic = kanal koji je okinuo PRVI (najmanji onset indeks) — info/debug. */
    int32_t ref = 0;
    if (onset[1] < onset[ref]) ref = 1;
    if (onset[2] < onset[ref]) ref = 2;
    dbgt_ref_mic = ref;

    /* 3. Raw snapshot prozora oko prvog onseta (za UART CSV / debugger). */
    {
        uint32_t first = (uint32_t)onset[ref];
        uint32_t half  = SAMPLES_PER_CHANNEL / 2u;
        uint32_t snap_off = (first > half) ? (first - half) : 0u;
        if (snap_off + SAMPLES_PER_CHANNEL > WIN_FRAMES) {
            snap_off = WIN_FRAMES - SAMPLES_PER_CHANNEL;
        }
        RawCapture_Snapshot(buf, snap_off);
    }

    /* 4. TDOA za geometriju — uvijek relativno na M1 (uzorci → sekunde).
     * tau_j = onset_j − onset_M1. (onset_M1 može i ne biti najmanji.) */
    int32_t d12 = onset[1] - onset[0];
    int32_t d13 = onset[2] - onset[0];

    /* Konzistentnost: onseti previše razmaknuti → fizički nemoguće, odbaci. */
    if (d12 > ONSET_MAX_SPREAD || d12 < -ONSET_MAX_SPREAD ||
        d13 > ONSET_MAX_SPREAD || d13 < -ONSET_MAX_SPREAD) return 0;

    float tau12_meas = (float)d12 * SAMPLE_PERIOD_S;
    float tau13_meas = (float)d13 * SAMPLE_PERIOD_S;
    dbgt_tau12_meas = tau12_meas;
    dbgt_tau13_meas = tau13_meas;

    /* 5. Korekcija ADC sekvencijalnog pomaka (M2 = RANK2 → +1, M3 = RANK3 → +2). */
    float tau12 = tau12_meas + 1.0f * CH_DELAY_S;
    float tau13 = tau13_meas + 2.0f * CH_DELAY_S;
    dbgt_tau12_corr = tau12;
    dbgt_tau13_corr = tau13;

    /* 6. Smjer u ravnini — [sx; sy] = −M_geom2 · [tau12; tau13]. */
    float sx = -(M_geom2[0][0] * tau12 + M_geom2[0][1] * tau13);
    float sy = -(M_geom2[1][0] * tau12 + M_geom2[1][1] * tau13);

    /* 7. Rekonstrukcija sz uz z >= 0. */
    float sxy2 = sx * sx + sy * sy;
    float sz;
    if (sxy2 > 1.0f) {
        float inv = 1.0f / sqrtf(sxy2);
        sx *= inv;
        sy *= inv;
        sz  = 0.0f;
    } else {
        sz = sqrtf(1.0f - sxy2);
    }
    dbgt_sx = sx; dbgt_sy = sy; dbgt_sz = sz;

    /* 8. Kutovi. */
    float az_deg = atan2f(sy, sx) * (180.0f / PI);
    if (az_deg < 0.0f) az_deg += 360.0f;

    float el_deg = asinf(sz) * (180.0f / PI);

    out->az_tenth = (int16_t)(az_deg * 10.0f);
    out->el_tenth = (int16_t)(el_deg * 10.0f);

    /* 9. Jakost — log mapa iz najvećeg odstupanja. */
    int32_t peak_max = max_abs[0];
    if (max_abs[1] > peak_max) peak_max = max_abs[1];
    if (max_abs[2] > peak_max) peak_max = max_abs[2];
    float str_f = 20.0f * log10f((float)peak_max + 1.0f);
    if (str_f < 1.0f)    str_f = 1.0f;
    if (str_f > 100.0f)  str_f = 100.0f;
    out->strength = (uint8_t)str_f;

    s_cooldown = DETECTION_COOLDOWN_FRAMES;
    return 1;
}
