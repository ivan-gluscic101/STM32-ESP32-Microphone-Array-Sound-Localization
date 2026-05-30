#include "sound_loc_3d.h"
#include "gcc_phat.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* Detekcijski prag — primjenjuje se na MIN RMS svih kanala (svi mikrofoni
 * moraju čuti zvuk da bi lagovi imali smisla). */
#define MIN_RMS_THRESHOLD       18.0f
#define MIN_RMS_PER_CHANNEL     (MIN_RMS_THRESHOLD * 0.3f)

#define SILENCE_RMS_THRESH      8.0f
#define SILENCE_FRAMES_REQUIRED 12

/* Minimalni omjer pika GCC-PHAT korelacije i mean|corr|.
 * Šum daje omjer ~1, pravi signal ≥ 2-3. Podesiti ako ima previše lažnih detekcija. */
#define GCC_MIN_PEAK_QUALITY    2.0f

typedef enum { DET_ARMED = 0, DET_HOLDOFF } det_state_t;
static det_state_t s_det_state   = DET_ARMED;
static uint16_t    s_silence_run = 0;

/* Kanalni nizovi — 4 × 512 × 4 B = 8 KB u BSS. */
static float s_ch0[SAMPLES_PER_CHANNEL];
static float s_ch1[SAMPLES_PER_CHANNEL];
static float s_ch2[SAMPLES_PER_CHANNEL];
static float s_ch3[SAMPLES_PER_CHANNEL];

/* Korelacijski buffer dijeljen kroz tri para (gcc_phat radi sekvencijalno). */
static float s_corr[SAMPLES_PER_CHANNEL];

/*
 * Prekalkulirana matrica M = c × inv(D) za tetraedar s bridom 10 cm.
 *
 * D matrica (lower triangular) — pozicije mikrofona M2, M3, M4 (M1 u ishodištu):
 *   [ MIC2_X   0        0      ]   [ 0.10   0        0       ]
 *   [ MIC3_X   MIC3_Y   0      ] = [ 0.05   0.0866   0       ]
 *   [ MIC4_X   MIC4_Y   MIC4_Z ]   [ 0.05   0.0289   0.0816  ]
 *
 * M[0][0] = c / MIC2_X                                = 343 / 0.10    = 3430
 * M[1][0] = -c · MIC3_X / (MIC2_X · MIC3_Y)           = -1980.31
 * M[1][1] = c / MIC3_Y                                = 343 / 0.0866  = 3960.62
 * M[2][0] = -c · (MIC4_X · MIC3_Y − MIC4_Y · MIC3_X)
 *           / (MIC2_X · MIC3_Y · MIC4_Z)              = -1400.29
 * M[2][1] = -c · MIC4_Y / (MIC3_Y · MIC4_Z)           = -1401.84
 * M[2][2] = c / MIC4_Z                                = 343 / 0.0816  = 4200.87
 */
static const float M_geom[3][3] = {
    {  3430.0000f,     0.0000f,     0.0000f },
    { -1980.3114f,  3960.6228f,     0.0000f },
    { -1400.2916f, -1400.2916f,  4200.8749f }
};

uint8_t LOC3D_Process(const uint16_t *buf, uint32_t frame_offset,
                      loc3d_result_t *out)
{
    /* 1. Deinterleave + DC removal + Hann prozor */
    GCC_ExtractChannels(buf, frame_offset, s_ch0, s_ch1, s_ch2, s_ch3);

    /* 2. Silence gate (MAX za detekciju, MIN za kvalitetu mjerenja) */
    float rms0 = GCC_RMS(s_ch0);
    float rms1 = GCC_RMS(s_ch1);
    float rms2 = GCC_RMS(s_ch2);
    float rms3 = GCC_RMS(s_ch3);

    float rms_max = rms0;
    if (rms1 > rms_max) rms_max = rms1;
    if (rms2 > rms_max) rms_max = rms2;
    if (rms3 > rms_max) rms_max = rms3;

    float rms_min = rms0;
    if (rms1 < rms_min) rms_min = rms1;
    if (rms2 < rms_min) rms_min = rms2;
    if (rms3 < rms_min) rms_min = rms3;

    /* HOLDOFF state machine — sprječava višestruke detekcije istog pljeska. */
    if (s_det_state == DET_HOLDOFF) {
        if (rms_max < SILENCE_RMS_THRESH) {
            if (++s_silence_run >= SILENCE_FRAMES_REQUIRED) {
                s_det_state   = DET_ARMED;
                s_silence_run = 0;
            }
        } else {
            s_silence_run = 0;
        }
        return 0;
    }

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

    /* Quality gate — odbaci frame ako je ijedan par korelacija ravna (šum). */
    if (qual12 < GCC_MIN_PEAK_QUALITY ||
        qual13 < GCC_MIN_PEAK_QUALITY ||
        qual14 < GCC_MIN_PEAK_QUALITY) return 0;

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

    /* 5. Geometrijsko rješenje — u_propagacije = M_geom · tau */
    float ux = M_geom[0][0] * tau12 + M_geom[0][1] * tau13 + M_geom[0][2] * tau14;
    float uy = M_geom[1][0] * tau12 + M_geom[1][1] * tau13 + M_geom[1][2] * tau14;
    float uz = M_geom[2][0] * tau12 + M_geom[2][1] * tau13 + M_geom[2][2] * tau14;

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

    /* 8. Jakost (500 LSB RMS → 100) */
    int32_t str = (int32_t)(rms_max * (100.0f / 500.0f));
    if (str < 1)   str = 1;
    if (str > 100) str = 100;
    out->strength = (uint8_t)str;

    /* 9. Aktiviraj HOLDOFF do sljedeće tišine */
    s_det_state   = DET_HOLDOFF;
    s_silence_run = 0;

    return 1;
}
