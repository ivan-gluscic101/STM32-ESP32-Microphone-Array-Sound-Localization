#include "sound_loc_3d.h"
#include "gcc_phat.h"
#include <math.h>

/* Minimalni RMS (nakon DC uklanjanja) koji aktivira lokalizaciju.
 * Idle šum tipično < 10 LSB RMS; tihi pljesak iz 2 m ~20-40 LSB; glasni ~100-500.
 * Prag se primjenjuje na MAX RMS po svim kanalima — ne ovisi o smjeru izvora. */
#define MIN_RMS_THRESHOLD     25.0f

/* ── Statički kanalni nizovi — NE smiju biti na stacku ────────────────────── */
/* Svaki = 512 × 4 B = 2 KB. Ukupno 8 KB u BSS segmentu (SRAM, uvijek prisutan).*/
static float s_ch0[SAMPLES_PER_CHANNEL];
static float s_ch1[SAMPLES_PER_CHANNEL];
static float s_ch2[SAMPLES_PER_CHANNEL];
static float s_ch3[SAMPLES_PER_CHANNEL];

uint8_t LOC3D_Process(const uint16_t *buf, uint32_t frame_offset,
                      loc3d_result_t *out)
{
    /* ── Korak 1: Deinterleave + DC removal (prozor počinje na frame_offset) ─ */
    GCC_ExtractChannels(buf, frame_offset, s_ch0, s_ch1, s_ch2, s_ch3);

    /* ── Korak 2: Silence gate — MAX RMS po svim kanalima ──────────────────── */
    /* Ne pretpostavljamo da je M1 najbliži izvoru. Govornik može biti bliže    */
    /* bilo kojem mikrofonu pa gate gleda najjači kanal u nizu.                 */
    float rms0 = GCC_RMS(s_ch0);
    float rms1 = GCC_RMS(s_ch1);
    float rms2 = GCC_RMS(s_ch2);
    float rms3 = GCC_RMS(s_ch3);
    float rms_max = rms0;
    if (rms1 > rms_max) rms_max = rms1;
    if (rms2 > rms_max) rms_max = rms2;
    if (rms3 > rms_max) rms_max = rms3;
    if (rms_max < MIN_RMS_THRESHOLD) return 0;

    /* ── Korak 3: GCC lags za 3 para (M1 = referentni mikrofon) ──────────── */
    /* Sub-sample (float) lag iz paraboličke interpolacije vrha korelacije.   */
    float lag12 = GCC_ComputeLag(s_ch0, s_ch1);
    float lag13 = GCC_ComputeLag(s_ch0, s_ch2);
    float lag14 = GCC_ComputeLag(s_ch0, s_ch3);

    /* ── Korak 4: Pretvorba laga u akustički TDOA [s] ─────────────────────── */
    /*                                                                          */
    /* Konvencija: tau_1j = T_1 - T_j (vrijeme dolaska na M1 minus na M_j).    */
    /* Sa ovom konvencijom je tau_1j > 0 kad zvuk PRIJE stigne na M_j (i      */
    /* formula tau_1j = (M_j - M_1) · u / c daje u koji pokazuje PREMA izvoru).*/
    /*                                                                          */
    /* Derivacija iz cross-correlation laga (ch0 = ref, ch_j = sig):          */
    /*   ch_0[s]   uzorkovan u t = s × T_s                                    */
    /*   ch_j[s+L] uzorkovan u t = (s+L) × T_s + (j-1) × CH_DELAY_S          */
    /*   Peak korelacije znači: oba uzorka su isti zvučni feature →           */
    /*     T_j - T_1 = L × T_s + (j-1) × CH_DELAY_S                          */
    /*   Pa konvencija tau_1j = T_1 - T_j daje:                               */
    /*     tau_1j = -L × T_s - (j-1) × CH_DELAY_S                            */
    float tau12 = -lag12 * SAMPLE_PERIOD_S - 1.0f * CH_DELAY_S;
    float tau13 = -lag13 * SAMPLE_PERIOD_S - 2.0f * CH_DELAY_S;
    float tau14 = -lag14 * SAMPLE_PERIOD_S - 3.0f * CH_DELAY_S;

    /* ── Korak 5: Analitičko rješavanje smjera u = (ux, uy, uz) ─────────── */
    /*                                                                         */
    /* Far-field model: tau_1j = (Mj − M1) · u / c   (M1 je u ishodištu)    */
    /*                                                                         */
    /* Sustav (donje trokutan → rješiv forward substitucijom):                */
    /*                                                                         */
    /*  [ MIC2_X    0       0    ] [ ux ]   [ tau12 · c ]                    */
    /*  [ MIC3_X   MIC3_Y   0    ] [ uy ] = [ tau13 · c ]                    */
    /*  [ MIC4_X   MIC4_Y  MIC4_Z] [ uz ]   [ tau14 · c ]                    */
    /*                                                                         */
    float c = SPEED_OF_SOUND;

    float ux = (tau12 * c) / MIC2_X;
    float uy = (tau13 * c - MIC3_X * ux) / MIC3_Y;
    float uz = (tau14 * c - MIC4_X * ux - MIC4_Y * uy) / MIC4_Z;

    /* ── Korak 6: Fizikalna validacija |u| ≤ 1 ───────────────────────────── */
    /* Za zvuk koji dolazi iz beskonačnosti |u| = 1 (jedinični vektor).       */
    /* |u| > 1 znači nekonzistentne TDOA vrijednosti (šum ili near-field).   */
    float mag2 = ux * ux + uy * uy + uz * uz;
    if (mag2 > 1.10f) return 0;   /* 1.05² ≈ 1.10 — 5% slack za kvantizaciju */

    /* ── Korak 7: Kutovi iz smjernog vektora ─────────────────────────────── */
    float az_rad = atan2f(uy, ux);
    float el_rad = atan2f(uz, sqrtf(ux * ux + uy * uy));

    out->az_tenth = (int16_t)(az_rad * (1800.0f / 3.14159265f));
    out->el_tenth = (int16_t)(el_rad * (1800.0f / 3.14159265f));

    /* ── Korak 8: Jakost — rms_max već izračunat u silence gate-u ────────── */
    /* 500 LSB RMS → jakost 100 (tipičan glasni pljeskaj bez klipinga) */
    int32_t str = (int32_t)(rms_max * (100.0f / 500.0f));
    if (str < 1)   str = 1;
    if (str > 100) str = 100;
    out->strength = (uint8_t)str;

    return 1;
}
