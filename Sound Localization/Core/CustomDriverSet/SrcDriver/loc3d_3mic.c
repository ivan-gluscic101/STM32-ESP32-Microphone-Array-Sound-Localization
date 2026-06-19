#include "loc3d_3mic.h"
#include "gcc_phat.h"
#include <math.h>

/*
 * loc3d_3mic.c — lokalizacija s 3 mikrofona (M1, M2, M3). M4 (ch3) se uzorkuje
 * ali se NE koristi (kanal je neispravan). Vidi loc3d_3mic.h za opis algoritma.
 *
 * Struktura prati sound_loc_3d.c; razlika je samo geometrijski dio: umjesto
 * 3x3 inverzije koristi se 2x2 (koplanaran niz), a elevacija se rekonstruira iz
 * uvjeta jediničnog vektora uz pretpostavku z >= 0.
 *
 * Debug varijable imaju prefiks dbg3_ da se ne sudaraju s dbg_* iz sound_loc_3d.c
 * ako se oba modula linkaju zajedno.
 */

/* Debug snapshot — postavi se pri svakoj detekciji. */
volatile float dbg3_tau12_meas, dbg3_tau13_meas;
volatile float dbg3_tau12_corr, dbg3_tau13_corr;
volatile float dbg3_qual12, dbg3_qual13;
volatile float dbg3_rms[4];
volatile float dbg3_sx, dbg3_sy, dbg3_sz;
volatile uint32_t dbg3_frame_offset;

/* Akumulacija TDOA mjerenja (za eksperimentalno određivanje CH_DELAY_S). */
volatile float    dbg3_tau_sum[2]   = { 0.0f, 0.0f };
volatile uint32_t dbg3_tau_count    = 0;
volatile float    dbg3_ch_delay_from_tau12;   /* = -avg(tau12_meas)       */
volatile float    dbg3_ch_delay_from_tau13;   /* = -avg(tau13_meas) / 2   */

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* Detekcijski pragovi — isti kao u sound_loc_3d.c (slabiji/neujednačeni mikrofoni).
 * RMS gate se sada radi SAMO nad kanalima M1/M2/M3 (M4 se ne gleda). */
#define MIN_RMS_THRESHOLD       40.0f
#define MIN_RMS_PER_CHANNEL     (MIN_RMS_THRESHOLD * 0.4f)

/* Minimalni omjer pika GCC-PHAT korelacije i srednje |korelacije| (peak/mean). */
#define GCC_MIN_PEAK_QUALITY    1.8f

/* Broj LOC3D_3MIC_Process poziva koji se ignoriraju nakon svake detekcije
 * (cooldown). 15 × 16 ms ≈ 240 ms. */
#define DETECTION_COOLDOWN_FRAMES  15

static uint16_t s_cooldown = 0;

/* Kanalni nizovi — ch3 (M4) se i dalje deinterleavea ali se ne koristi. */
static float s_ch0[SAMPLES_PER_CHANNEL];
static float s_ch1[SAMPLES_PER_CHANNEL];
static float s_ch2[SAMPLES_PER_CHANNEL];
static float s_ch3[SAMPLES_PER_CHANNEL];

/* Korelacijski buffer dijeljen kroz parove. */
static float s_corr[SAMPLES_PER_CHANNEL];

/*
 * M_geom2 = c · inv(D2), gdje su retci D2 baseline vektori (Mj − M1) u ravnini:
 *   D2 = [ M2x−M1x, M2y−M1y ;
 *          M3x−M1x, M3y−M1y ]
 * Veza (tau = T_j − T_1): za smjer PREMA izvoru s vrijedi  D2 · [sx;sy] = −c·tau,
 *   pa je [sx;sy] = −c·inv(D2)·tau = −M_geom2·tau.
 * Računa se u LOC3D_3MIC_Init() iz MIC*_{X,Y} (audio_common.h).
 */
static float M_geom2[2][2];

/* Analitička inverzija 2×2. */
static void invert2x2(const float m[2][2], float inv[2][2])
{
    float det  = m[0][0] * m[1][1] - m[0][1] * m[1][0];
    float idet = (fabsf(det) > 1e-12f) ? (1.0f / det) : 0.0f;

    inv[0][0] =  m[1][1] * idet;
    inv[0][1] = -m[0][1] * idet;
    inv[1][0] = -m[1][0] * idet;
    inv[1][1] =  m[0][0] * idet;
}

void LOC3D_3MIC_Init(void)
{
    /* Samo X/Y komponente — niz je u ravnini z = 0. */
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

/* Debug brojač — broji koliko je puta LOC3D_3MIC_Process pozvan. */
volatile uint32_t dbg3_loc3d_call_count = 0;
volatile float    dbg3_last_rms[4];

/*
 * Sliding-window energy search — identično sound_loc_3d.c. Energija se i dalje
 * računa preko sva 4 kanala; M4 je neispravan ali njegova energija ne kvari
 * lociranje vrha bitno (DC/šum je relativno konstantan). Ako se pokaže da M4
 * remeti, lako se ovdje preskoči ch == 3.
 */
static uint32_t find_peak_offset(const uint16_t *buf)
{
    const uint32_t TOTAL = 2u * SAMPLES_PER_CHANNEL;
    const uint32_t PROBE = 16u;   /* ~250 µs @ 64 kHz */
    const uint32_t HALF  = SAMPLES_PER_CHANNEL / 2u;

    /* Energija prvog prozora — samo kanali 0..2 (M4 ignoriran). */
    uint32_t win_e = 0u;
    for (uint32_t i = 0u; i < PROBE; i++) {
        const uint16_t *s = &buf[i * NUM_CH];
        for (uint32_t ch = 0u; ch < 3u; ch++) {
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

    if (best_frame < HALF)              return 0u;
    if (best_frame + HALF > TOTAL)      return TOTAL - SAMPLES_PER_CHANNEL;
    return best_frame - HALF;
}

uint8_t LOC3D_3MIC_Process(const uint16_t *buf, loc3d_3mic_result_t *out)
{
    dbg3_loc3d_call_count++;

    /* Cooldown — preskači obradu odmah, bez skupog peak-findinga */
    if (s_cooldown > 0) { s_cooldown--; return 0; }

    /* 1. Nađi gdje je pljesak — GCC prozor centriran na energetski vrh */
    uint32_t frame_offset = find_peak_offset(buf);
    dbg3_frame_offset = frame_offset;

    /* 2. Deinterleave + DC removal + Hann prozor (ch3 = M4 se puni ali ne koristi) */
    GCC_ExtractChannels(buf, frame_offset, s_ch0, s_ch1, s_ch2, s_ch3);

    /* 3. RMS gate — SAMO M1/M2/M3 (M4 neispravan, isključen iz gatea) */
    float rms0 = GCC_RMS(s_ch0);
    float rms1 = GCC_RMS(s_ch1);
    float rms2 = GCC_RMS(s_ch2);
    float rms3 = GCC_RMS(s_ch3);   /* samo za debug uvid */

    dbg3_last_rms[0] = rms0; dbg3_last_rms[1] = rms1;
    dbg3_last_rms[2] = rms2; dbg3_last_rms[3] = rms3;

    float rms_max = rms0;
    if (rms1 > rms_max) rms_max = rms1;
    if (rms2 > rms_max) rms_max = rms2;

    float rms_min = rms0;
    if (rms1 < rms_min) rms_min = rms1;
    if (rms2 < rms_min) rms_min = rms2;

    if (rms_max < MIN_RMS_THRESHOLD)    return 0;
    if (rms_min < MIN_RMS_PER_CHANNEL)  return 0;

    /* 4. GCC-PHAT za dva para (M1 = referentni) */
    float qual12, qual13;

    GCC_PHAT(s_ch0, s_ch1, s_corr);
    float tau12_meas = GCC_FindTDOA(s_corr, &qual12);

    GCC_PHAT(s_ch0, s_ch2, s_corr);
    float tau13_meas = GCC_FindTDOA(s_corr, &qual13);

    /* Debug: kvaliteta oba para — PRIJE gatea da se vidi koji par padne. */
    dbg3_qual12 = qual12;
    dbg3_qual13 = qual13;

    /* Quality gate — odbaci frame ako je ijedan par korelacija ravna (šum). */
    if (qual12 < GCC_MIN_PEAK_QUALITY ||
        qual13 < GCC_MIN_PEAK_QUALITY) return 0;

    /* Detekcija prošla — snimi sirovi sadržaj poravnatog prozora za debug. */
    GCC_SnapshotRaw(buf, frame_offset);

    /* Debug snapshot ključnih veličina. */
    dbg3_tau12_meas = tau12_meas;
    dbg3_tau13_meas = tau13_meas;
    dbg3_rms[0] = rms0; dbg3_rms[1] = rms1; dbg3_rms[2] = rms2; dbg3_rms[3] = rms3;

    /* Akumuliraj za CH_DELAY estimaciju */
    dbg3_tau_sum[0] += tau12_meas;
    dbg3_tau_sum[1] += tau13_meas;
    dbg3_tau_count++;
    if (dbg3_tau_count > 0) {
        float n = (float)dbg3_tau_count;
        dbg3_ch_delay_from_tau12 = -dbg3_tau_sum[0] / n;
        dbg3_ch_delay_from_tau13 = -dbg3_tau_sum[1] / (2.0f * n);
    }

    /* 5. Korekcija ADC sekvencijalnog pomaka.
     *   tau_meas = T_j − T_1 − (j−1)·CH_DELAY  →  T_j − T_1 = tau_meas + (j−1)·CH_DELAY
     *   M2 = RANK2 → (j−1)=1 ; M3 = RANK3 → (j−1)=2 */
    float tau12 = tau12_meas + 1.0f * CH_DELAY_S;
    float tau13 = tau13_meas + 2.0f * CH_DELAY_S;

    dbg3_tau12_corr = tau12;
    dbg3_tau13_corr = tau13;

    /* 6. Geometrijsko rješenje u ravnini — [sx; sy] = −M_geom2 · [tau12; tau13].
     * Predznak (−) jer računamo smjer PREMA izvoru: D2·s = −c·tau. */
    float sx = -(M_geom2[0][0] * tau12 + M_geom2[0][1] * tau13);
    float sy = -(M_geom2[1][0] * tau12 + M_geom2[1][1] * tau13);

    /* 7. Rekonstrukcija sz uz pretpostavku z >= 0. (sx, sy) su komponente
     * jediničnog smjera u ravnini, pa je sx^2 + sy^2 <= 1 idealno. Mjerni šum
     * ili izvor blizu horizonta mogu dati sxy^2 > 1 — tada clampamo na 1 (sz=0).
     *
     * Normaliziramo (sx, sy) tako da cijeli (sx, sy, sz) bude jedinični:
     *   ako je sxy2 > 1, skaliramo (sx, sy) na jediničnu kružnicu i sz = 0. */
    float sxy2 = sx * sx + sy * sy;
    float sz;
    if (sxy2 > 1.0f) {
        float inv = 1.0f / sqrtf(sxy2);
        sx *= inv;
        sy *= inv;
        sz  = 0.0f;
    } else {
        sz = sqrtf(1.0f - sxy2);   /* z >= 0 pretpostavka */
    }

    dbg3_sx = sx; dbg3_sy = sy; dbg3_sz = sz;

    /* 8. Kutovi.
     * Azimut: atan2 → wrap u [0,360). Elevacija: asin(sz) ∈ [0, +90] (z>=0). */
    float az_deg = atan2f(sy, sx) * (180.0f / PI);
    if (az_deg < 0.0f) az_deg += 360.0f;   /* (-180,180] → [0,360) */

    float el_deg = asinf(sz) * (180.0f / PI);   /* 0 .. +90 */

    out->az_tenth = (int16_t)(az_deg * 10.0f);   /* 0 .. 3600 */
    out->el_tenth = (int16_t)(el_deg * 10.0f);   /* 0 .. +900 */

    /* 9. Jakost — log mapa (dB-like), kao u glavnoj verziji. */
    float str_f = 20.0f * log10f(rms_max + 1.0f);
    if (str_f < 1.0f)    str_f = 1.0f;
    if (str_f > 100.0f)  str_f = 100.0f;
    out->strength = (uint8_t)str_f;

    s_cooldown = DETECTION_COOLDOWN_FRAMES;
    return 1;
}
