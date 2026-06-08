#include "sound_loc_3d.h"
#include "gcc_phat.h"
#include <math.h>

/* Debug snapshot — postavi se pri svakoj detekciji.
 * Provjeravaj u debuggeru da vidiš stvarne vrijednosti algoritma. */
volatile float dbg_tau12_meas, dbg_tau13_meas, dbg_tau14_meas;
volatile float dbg_tau12_corr, dbg_tau13_corr, dbg_tau14_corr;
volatile float dbg_qual12, dbg_qual13, dbg_qual14;
volatile float dbg_rms[4];
volatile float dbg_ux, dbg_uy, dbg_uz;
volatile uint32_t dbg_frame_offset;

/* Akumulacija TDOA mjerenja — koristi se za eksperimentalno određivanje
 * CH_DELAY_S. Spoji 4 ADC pina paralelno na isti izvor (npr. M1), pljesni
 * mnogo puta, pa pročitaj dbg_ch_delay_from_tau12/13/14 vrijednosti.
 *
 *   Bez akustike (svi kanali = isti signal):
 *     tau12_meas = -1 · CH_DELAY
 *     tau13_meas = -2 · CH_DELAY
 *     tau14_meas = -3 · CH_DELAY
 *
 *   Pa eksperimentalni CH_DELAY = -avg(tau12) = -avg(tau13)/2 = -avg(tau14)/3
 *   Sve tri vrijednosti moraju biti slične — to je sanity check. */
volatile float    dbg_tau_sum[3]   = { 0.0f, 0.0f, 0.0f };
volatile uint32_t dbg_tau_count    = 0;
volatile float    dbg_ch_delay_from_tau12;   /* = -avg(tau12_meas)       */
volatile float    dbg_ch_delay_from_tau13;   /* = -avg(tau13_meas) / 2   */
volatile float    dbg_ch_delay_from_tau14;   /* = -avg(tau14_meas) / 3   */

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* Detekcijski prag — snižen za stvarne (slabije / neujednačene) mikrofone.
 * Novi niz daje manju amplitudu nego prijašnji, pa je prag 20 gušio pljeskove.
 * MIN_RMS_THRESHOLD = glasnoća najjačeg kanala potrebna da se uopće pokrene
 * obrada; MIN_RMS_PER_CHANNEL je labaviji (0.2×) da JEDAN slabiji mikrofon ne
 * obori cijelu detekciju. Diži natrag ako šum počne okidati lažne detekcije. */
#define MIN_RMS_THRESHOLD       10.0f
#define MIN_RMS_PER_CHANNEL     (MIN_RMS_THRESHOLD * 0.2f)

/* Minimalni omjer pika GCC-PHAT korelacije i srednje |korelacije| (peak/mean).
 * Trenutačno 0.0 → gate je ISKLJUČEN: propušta svaki prozor, uključujući šum.
 * Vrati na ~1.3–1.5 nakon što potvrdiš detekciju, da odsiječeš čisti šum. */
#define GCC_MIN_PEAK_QUALITY    0.0f

/* Broj LOC3D_Process poziva koji se ignoriraju nakon svake detekcije.
 * task_manager.c zove LOC3D_Process jednom po half-bufferu = 16 ms.
 * 19 × 16 ms ≈ 304 ms cooldown. */
#define DETECTION_COOLDOWN_FRAMES  19

static uint16_t s_cooldown = 0;

/* Kanalni nizovi — 4 × 1024 × 4 B = 16 KB u BSS. */
static float s_ch0[SAMPLES_PER_CHANNEL];
static float s_ch1[SAMPLES_PER_CHANNEL];
static float s_ch2[SAMPLES_PER_CHANNEL];
static float s_ch3[SAMPLES_PER_CHANNEL];

/* Korelacijski buffer dijeljen kroz tri para (gcc_phat radi sekvencijalno). */
static float s_corr[SAMPLES_PER_CHANNEL];

/*
 * M_geom = c · inv(D); retci D su baseline vektori (Mj − M1):
 *   D = [ M2−M1 ; M3−M1 ; M4−M1 ]
 * Veza (tau = T_j − T_1): za smjer PREMA izvoru s vrijedi  D · s = −c · tau,
 *   pa je s = −c·inv(D)·tau = −M_geom·tau. Kod računa u = M_geom·tau (= smjer
 *   propagacije = −s) i uzima s = −u (korak 6).
 * Računa se u LOC3D_Init() iz MIC*_{X,Y,Z} (audio_common.h) — promjena
 * geometrije se automatski propagira, bez ručnog preračunavanja matrice. */
static float M_geom[3][3];

/* Analitička inverzija 3×3 (adjugate / determinanta). */
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

void LOC3D_Init(void)
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

/* Debug brojač — broji koliko je puta LOC3D_Process pozvan. Ako ostane 0,
 * pipeline ne dolazi ovdje. */
volatile uint32_t dbg_loc3d_call_count = 0;
volatile float    dbg_last_rms[4];

/*
 * Sliding-window energy search — traži 16-frame prozor s najvećom energijom
 * u cijelom sliding_buf (2 × SAMPLES_PER_CHANNEL frejmova), zatim centrira
 * GCC prozor na taj vrh.
 *
 * Inkrementalni algoritam: O(N × NUM_CH) umjesto O(N × PROBE × NUM_CH).
 * Na 2048 frejmova × 4 kanala ≈ 8 K operacija → < 0.1 ms @ 170 MHz.
 */
static uint32_t find_peak_offset(const uint16_t *buf)
{
    const uint32_t TOTAL = 2u * SAMPLES_PER_CHANNEL;
    const uint32_t PROBE = 16u;   /* ~250 µs @ 64 kHz — dovoljno za detekciju */
    const uint32_t HALF  = SAMPLES_PER_CHANNEL / 2u;

    /* Izračunaj energiju prvog prozora */
    uint32_t win_e = 0u;
    for (uint32_t i = 0u; i < PROBE; i++) {
        const uint16_t *s = &buf[i * NUM_CH];
        for (uint32_t ch = 0u; ch < (uint32_t)NUM_CH; ch++) {
            int32_t v = (int32_t)s[ch] - 2048;
            win_e += (uint32_t)((uint32_t)(v * v) >> 6);
        }
    }
    uint32_t best_e     = win_e;
    uint32_t best_frame = PROBE / 2u;

    /* Inkrementalno kliži prozor */
    for (uint32_t f = 1u; f + PROBE <= TOTAL; f++) {
        for (uint32_t ch = 0u; ch < (uint32_t)NUM_CH; ch++) {
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

    /* Centriraj GCC prozor na vrh energije, stisnuti u valjane granice */
    if (best_frame < HALF)              return 0u;
    if (best_frame + HALF > TOTAL)      return TOTAL - SAMPLES_PER_CHANNEL;
    return best_frame - HALF;
}

uint8_t LOC3D_Process(const uint16_t *buf, loc3d_result_t *out)
{
    dbg_loc3d_call_count++;

    /* Cooldown — preskači obradu odmah, bez skupog peak-findinga */
    if (s_cooldown > 0) { s_cooldown--; return 0; }

    /* 1. Nađi gdje je pljesak — GCC prozor centriran na energetski vrh */
    uint32_t frame_offset = find_peak_offset(buf);
    dbg_frame_offset = frame_offset;

    /* 2. Deinterleave + DC removal + Hann prozor na poravnatom offsetu */
    GCC_ExtractChannels(buf, frame_offset, s_ch0, s_ch1, s_ch2, s_ch3);

    /* 3. RMS gate */
    float rms0 = GCC_RMS(s_ch0);
    float rms1 = GCC_RMS(s_ch1);
    float rms2 = GCC_RMS(s_ch2);
    float rms3 = GCC_RMS(s_ch3);

    dbg_last_rms[0] = rms0; dbg_last_rms[1] = rms1;
    dbg_last_rms[2] = rms2; dbg_last_rms[3] = rms3;

    float rms_max = rms0;
    if (rms1 > rms_max) rms_max = rms1;
    if (rms2 > rms_max) rms_max = rms2;
    if (rms3 > rms_max) rms_max = rms3;

    float rms_min = rms0;
    if (rms1 < rms_min) rms_min = rms1;
    if (rms2 < rms_min) rms_min = rms2;
    if (rms3 < rms_min) rms_min = rms3;

    if (rms_max < MIN_RMS_THRESHOLD)    return 0;
    if (rms_min < MIN_RMS_PER_CHANNEL)  return 0;

    /* 3. GCC-PHAT za tri para (M1 = referentni) */
    float qual12, qual13, qual14;

    GCC_PHAT(s_ch0, s_ch1, s_corr);
    float tau12_meas = GCC_FindTDOA(s_corr, &qual12);

    GCC_PHAT(s_ch0, s_ch2, s_corr);
    float tau13_meas = GCC_FindTDOA(s_corr, &qual13);

    GCC_PHAT(s_ch0, s_ch3, s_corr);
    float tau14_meas = GCC_FindTDOA(s_corr, &qual14);

    /* Debug: kvaliteta sva tri para — postavljeno PRIJE gatea da se u debuggeru
     * vidi i kad neki par padne (inače ne znaš koji mikrofon ruši detekciju). */
    dbg_qual12 = qual12;
    dbg_qual13 = qual13;
    dbg_qual14 = qual14;

    /* Quality gate — odbaci frame ako je ijedan par korelacija ravna (šum). */
    if (qual12 < GCC_MIN_PEAK_QUALITY ||
        qual13 < GCC_MIN_PEAK_QUALITY ||
        qual14 < GCC_MIN_PEAK_QUALITY) return 0;

    /* Detekcija prošla — snimi sirovi sadržaj poravnatog prozora za debug. */
    GCC_SnapshotRaw(buf, frame_offset);

    /* Debug snapshot svih ključnih veličina algoritma. */
    dbg_tau12_meas = tau12_meas;
    dbg_tau13_meas = tau13_meas;
    dbg_tau14_meas = tau14_meas;
    dbg_rms[0] = rms0; dbg_rms[1] = rms1; dbg_rms[2] = rms2; dbg_rms[3] = rms3;
    /* dbg_frame_offset je već postavljen gore (find_peak_offset rezultat) */

    /* Akumuliraj za CH_DELAY estimaciju */
    dbg_tau_sum[0] += tau12_meas;
    dbg_tau_sum[1] += tau13_meas;
    dbg_tau_sum[2] += tau14_meas;
    dbg_tau_count++;
    if (dbg_tau_count > 0) {
        float n = (float)dbg_tau_count;
        dbg_ch_delay_from_tau12 = -dbg_tau_sum[0] / n;
        dbg_ch_delay_from_tau13 = -dbg_tau_sum[1] / (2.0f * n);
        dbg_ch_delay_from_tau14 = -dbg_tau_sum[2] / (3.0f * n);
    }

    /* 4. Korekcija ADC sekvencijalnog pomaka.
     *
     *   t_sample(M_k, s) = s·Ts + (k-1)·CH_DELAY_S
     *
     *   Akustički događaj na M1 u t=T1, na M_j u t=T_j. M_j vidi događaj
     *   na sample indeksu s_j = (T_j - (j-1)·CH_DELAY)/Ts (RANIJI indeks
     *   jer je njegovo sampliranje pomaknuto u budućnost).
     *
     *   GCC-PHAT mjeri:  tau_meas = (s_j - s_1)·Ts = T_j - T_1 - (j-1)·CH_DELAY
     *   Stvarni akustički delay:  T_j - T_1 = tau_meas + (j-1)·CH_DELAY  */
    float tau12 = tau12_meas + 1.0f * CH_DELAY_S;
    float tau13 = tau13_meas + 2.0f * CH_DELAY_S;
    float tau14 = tau14_meas + 3.0f * CH_DELAY_S;

    /* Debug: tau nakon CH_DELAY korekcije */
    dbg_tau12_corr = tau12;
    dbg_tau13_corr = tau13;
    dbg_tau14_corr = tau14;

    /* 5. Geometrijsko rješenje — u_propagacije = M_geom · tau */
    float ux = M_geom[0][0] * tau12 + M_geom[0][1] * tau13 + M_geom[0][2] * tau14;
    float uy = M_geom[1][0] * tau12 + M_geom[1][1] * tau13 + M_geom[1][2] * tau14;
    float uz = M_geom[2][0] * tau12 + M_geom[2][1] * tau13 + M_geom[2][2] * tau14;

    dbg_ux = ux; dbg_uy = uy; dbg_uz = uz;

    /* 6. Vektor smjera PREMA izvoru = −u_propagacije, normaliziran */
    float sx = -ux, sy = -uy, sz = -uz;
    float norm = sqrtf(sx * sx + sy * sy + sz * sz);
    if (norm < 1e-9f) return 0;
    sx /= norm; sy /= norm; sz /= norm;

    /* 7. Kutovi (azimut u rasponu [-180, 180], elevacija [-90, 90]) */
    float az_rad = atan2f(sy, sx);
    float el_rad = asinf(sz);

    out->az_tenth = (int16_t)(az_rad * (1800.0f / PI));
    out->el_tenth = (int16_t)(el_rad * (1800.0f / PI));

    /* 8. Jakost — log mapa (dB-like) za vizualizaciju (pljesak RMS≈30..500).
     * 20·log10(rms+1): RMS=20→~26, RMS=100→~40, RMS=500→~54 (ne doseže 100). */
    float str_f = 20.0f * log10f(rms_max + 1.0f);
    if (str_f < 1.0f)    str_f = 1.0f;
    if (str_f > 100.0f)  str_f = 100.0f;
    out->strength = (uint8_t)str_f;

    s_cooldown = DETECTION_COOLDOWN_FRAMES;
    return 1;
}
