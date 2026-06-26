#include "sound_loc3d_4mic_time.h"
#include "gcc_phat.h"   /* samo za GCC_SnapshotRaw (memcpy raw uzoraka za UART debug) */
#include <math.h>

/*
 * sound_loc3d_4mic_time.c — time-domain TDOA lokalizacija (M1, M2, M3, M4) PRAGOM.
 *
 * Princip (bez FFT, bez micanja DC-a, bez energy peak finda):
 *   - DC je fiksno DC_LEVEL. Uzorak "okida" kad |sample − DC| > THR.
 *   - Idemo redom kroz uzorke od početka buffera. Za svaki kanal (M1..M4)
 *     bilježimo PRVI uzorak u kojem okine. ref_mic = onaj koji okine prvi.
 *   - Razlika onset indeksa između kanala = TDOA (u uzorcima). Iz 3 TDOA
 *     (M1↔M2, M1↔M3, M1↔M4) rješava se PUNI 3D sustav (3×3) → smjer (sx,sy,sz).
 *
 * Razlika u odnosu na loc3d_3mic_time.c:
 *   3-mic koristi 2D M_geom2 i rekonstruira sz uz pretpostavku z >= 0. Ovdje M4
 *   daje 3. nezavisnu jednadžbu pa dobivamo PRAVI predznak elevacije (z može < 0).
 *
 * Geometrija/predznaci: TDOA se za geometriju uvijek formira relativno na M1
 * (tau1j = onset_Mj − onset_M1), neovisno o tome koji je kanal okinuo prvi.
 *
 * Debug snapshot (dbg4_*) — čita se u debuggeru, ne ide na UART.
 */

volatile int32_t  dbg4_onset[4];                            /* onset indeksi (uzorci), -1 = nije okinuo */
volatile int32_t  dbg4_ref_mic;                             /* 0..3 = M1..M4 okinuo prvi */
volatile float    dbg4_tau12_meas, dbg4_tau13_meas, dbg4_tau14_meas;  /* prije CH_DELAY korekcije */
volatile float    dbg4_tau12_corr, dbg4_tau13_corr, dbg4_tau14_corr;  /* poslije korekcije        */
volatile int32_t  dbg4_peak_abs[4];                         /* max |signal-DC| po kanalu */
volatile float    dbg4_sx, dbg4_sy, dbg4_sz;

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* Ukupan broj frejmova u sliding bufferu. */
#define WIN_FRAMES   (2u * SAMPLES_PER_CHANNEL)

/* Fiksna "DC" razina i prag odstupanja. Uzorak okida kad |sample − DC| > THR. */
#define DC_LEVEL         2050
#define THRESHOLD_LEVEL  250

/* Maksimalna razlika onseta između kanala (uzoraka). Veće → odbaci (nekonzistentno).
 * Najveći baseline = brid tetraedra; vidi TDOA_MAX_SAMPLES (+ rezerva). */
#define ONSET_MAX_SPREAD  (TDOA_MAX_SAMPLES + 6)

/* Cooldown nakon detekcije (broj poziva Process). Frame = half-buffer =
 * SAMPLES_PER_CHANNEL/fs. Pri 192 kHz: 1024/192k ≈ 5.33 ms; 45 × 5.33 ≈ 240 ms —
 * propusti rep pljeska i prve odjeke. (Skalira s fs: digni ako povisiš fs.) */
#define DETECTION_COOLDOWN_FRAMES  45

static uint16_t s_cooldown = 0;

/* M_geom = c·inv(D); retci D su baseline vektori (Mj − M1):
 *   D = [ M2−M1 ; M3−M1 ; M4−M1 ]
 * Smjer propagacije u = M_geom·tau; smjer PREMA izvoru s = −u (korak 6). */
static float M_geom[3][3];

/* Analitička inverzija 3×3 (adjugate / determinanta) — kao u sound_loc_3d.c. */
static void invert3x3(const float m[3][3], float inv[3][3])
{
    float det =
        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    float idet = (fabsf(det) > 1e-12f) ? (1.0f / det) : 0.0f;

    inv[0][0] =  (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * idet;
    inv[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * idet;
    inv[0][2] =  (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * idet;
    inv[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * idet;
    inv[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * idet;
    inv[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * idet;
    inv[2][0] =  (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * idet;
    inv[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * idet;
    inv[2][2] =  (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * idet;
}

void LOC3D_4MIC_TIME_Init(void)
{
    const float D[3][3] = {
        { MIC2_X - MIC1_X, MIC2_Y - MIC1_Y, MIC2_Z - MIC1_Z },
        { MIC3_X - MIC1_X, MIC3_Y - MIC1_Y, MIC3_Z - MIC1_Z },
        { MIC4_X - MIC1_X, MIC4_Y - MIC1_Y, MIC4_Z - MIC1_Z }
    };
    float Dinv[3][3];
    invert3x3(D, Dinv);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            M_geom[i][j] = SPEED_OF_SOUND * Dinv[i][j];
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

uint8_t LOC3D_4MIC_TIME_Process(const uint16_t *buf, loc3d_4mic_time_result_t *out)
{
    if (s_cooldown > 0) { s_cooldown--; return 0; }

    /* 1. Onset (prvi prelazak praga) i max odstupanje po kanalu M1..M4. */
    int32_t max_abs[4];
    int32_t onset[4];
    for (uint32_t ch = 0u; ch < 4u; ch++) {
        onset[ch] = find_onset(buf, ch, &max_abs[ch]);
        dbg4_onset[ch]    = onset[ch];
        dbg4_peak_abs[ch] = max_abs[ch];
    }

    /* 2. SVI kanali moraju okinuti (preći prag) — inače nema valjane detekcije. */
    if (onset[0] < 0 || onset[1] < 0 || onset[2] < 0 || onset[3] < 0) return 0;

    /* ref_mic = kanal koji je okinuo PRVI (najmanji onset indeks) — info/debug. */
    int32_t ref = 0;
    for (int32_t k = 1; k < 4; k++) {
        if (onset[k] < onset[ref]) ref = k;
    }
    dbg4_ref_mic = ref;

    /* 3. Raw snapshot prozora oko prvog onseta (za UART CSV / debugger). */
    {
        uint32_t first = (uint32_t)onset[ref];
        uint32_t half  = SAMPLES_PER_CHANNEL / 2u;
        uint32_t snap_off = (first > half) ? (first - half) : 0u;
        if (snap_off + SAMPLES_PER_CHANNEL > WIN_FRAMES) {
            snap_off = WIN_FRAMES - SAMPLES_PER_CHANNEL;
        }
        GCC_SnapshotRaw(buf, snap_off);
    }

    /* 4. TDOA za geometriju — uvijek relativno na M1 (uzorci → sekunde).
     * tau1j = onset_Mj − onset_M1. (onset_M1 može i ne biti najmanji.) */
    int32_t d12 = onset[1] - onset[0];
    int32_t d13 = onset[2] - onset[0];
    int32_t d14 = onset[3] - onset[0];

    /* Konzistentnost: onseti previše razmaknuti → fizički nemoguće, odbaci. */
    if (d12 > ONSET_MAX_SPREAD || d12 < -ONSET_MAX_SPREAD ||
        d13 > ONSET_MAX_SPREAD || d13 < -ONSET_MAX_SPREAD ||
        d14 > ONSET_MAX_SPREAD || d14 < -ONSET_MAX_SPREAD) return 0;

    float tau12_meas = (float)d12 * SAMPLE_PERIOD_S;
    float tau13_meas = (float)d13 * SAMPLE_PERIOD_S;
    float tau14_meas = (float)d14 * SAMPLE_PERIOD_S;
    dbg4_tau12_meas = tau12_meas;
    dbg4_tau13_meas = tau13_meas;
    dbg4_tau14_meas = tau14_meas;

    /* 5. Korekcija ADC sekvencijalnog pomaka (M2=RANK2→+1, M3=RANK3→+2, M4=RANK4→+3). */
    float tau12 = tau12_meas + 1.0f * CH_DELAY_S;
    float tau13 = tau13_meas + 2.0f * CH_DELAY_S;
    float tau14 = tau14_meas + 3.0f * CH_DELAY_S;
    dbg4_tau12_corr = tau12;
    dbg4_tau13_corr = tau13;
    dbg4_tau14_corr = tau14;

    /* 6. Pun 3D smjer — u_propagacije = M_geom · tau; smjer PREMA izvoru s = −u. */
    float ux = M_geom[0][0] * tau12 + M_geom[0][1] * tau13 + M_geom[0][2] * tau14;
    float uy = M_geom[1][0] * tau12 + M_geom[1][1] * tau13 + M_geom[1][2] * tau14;
    float uz = M_geom[2][0] * tau12 + M_geom[2][1] * tau13 + M_geom[2][2] * tau14;

    float sx = -ux, sy = -uy, sz = -uz;
    float norm = sqrtf(sx * sx + sy * sy + sz * sz);
    if (norm < 1e-9f) return 0;
    sx /= norm; sy /= norm; sz /= norm;

    /* 7. PRETPOSTAVKA z >= 0 — izvor je iznad ili u ravnini niza, nikad ispod.
     * M4 (RANK4) je gruba i šumovita pa znade dati lažnu negativnu elevaciju
     * ("pljesneš iznad, detektira dolje"). Kad puni 3D sustav vrati sz < 0,
     * NE vjerujemo z-komponenti: zadržimo (sx, sy) iz horizontalnog rješenja i
     * rekonstruiramo sz iz uvjeta |s| = 1 uz z >= 0 (kao 3-mic verzija). */
    if (sz < 0.0f) {
        float sxy2 = sx * sx + sy * sy;
        if (sxy2 > 1.0f) {
            float inv = 1.0f / sqrtf(sxy2);   /* prejak horizontalni TDOA → na horizont */
            sx *= inv; sy *= inv;
            sz  = 0.0f;
        } else {
            sz = sqrtf(1.0f - sxy2);          /* zrcali negativnu elevaciju u >= 0 */
        }
    }
    dbg4_sx = sx; dbg4_sy = sy; dbg4_sz = sz;

    /* 8. Kutovi. Azimut wrap [0,360); elevacija [0,+90] (z >= 0 pretpostavka). */
    float az_deg = atan2f(sy, sx) * (180.0f / PI);
    if (az_deg < 0.0f) az_deg += 360.0f;

    float el_deg = asinf(sz) * (180.0f / PI);

    out->az_tenth = (int16_t)(az_deg * 10.0f);
    out->el_tenth = (int16_t)(el_deg * 10.0f);

    /* 9. Jakost — log mapa iz najvećeg odstupanja po svim kanalima. */
    int32_t peak_max = max_abs[0];
    for (int32_t k = 1; k < 4; k++) {
        if (max_abs[k] > peak_max) peak_max = max_abs[k];
    }
    float str_f = 20.0f * log10f((float)peak_max + 1.0f);
    if (str_f < 1.0f)    str_f = 1.0f;
    if (str_f > 100.0f)  str_f = 100.0f;
    out->strength = (uint8_t)str_f;

    s_cooldown = DETECTION_COOLDOWN_FRAMES;
    return 1;
}
