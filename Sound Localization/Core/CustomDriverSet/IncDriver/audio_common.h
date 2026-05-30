#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

/* ── Buffer layout ─────────────────────────────────────────────────────────── */
#define NUM_CH                4
#define SAMPLES_PER_CHANNEL   512
#define HALF_SIZE             SAMPLES_PER_CHANNEL
#define HALF_BUFFER           (NUM_CH * SAMPLES_PER_CHANNEL)   /* 2048 uzoraka */
#define FULL_BUFFER           (HALF_BUFFER * 2)                /* 4096 uzoraka */

/* ── Vremenski parametri ───────────────────────────────────────────────────── */
/* TIM8 ARR=2655 @ 170 MHz → okida ADC svakih 15.625 µs = 64 kHz po kanalu   */
#define SAMPLE_RATE_HZ        64000
#define SAMPLE_PERIOD_S       (1.0f / SAMPLE_RATE_HZ)          /* 15.625 µs   */

/* ── ADC sekvencijalni offset ─────────────────────────────────────────────── */
/* ADC clock = PCLK/4 = 170 MHz/4 = 42.5 MHz → perioda 23.53 ns              */
/* Svaki kanal = 24.5 (sampling) + 12.5 (conv) = 37 ciklusa = 870.6 ns       */
#define CH_DELAY_S            870.6e-9f

/* ── Akustika ──────────────────────────────────────────────────────────────── */
#define SPEED_OF_SOUND        343.0f    /* m/s pri ~20°C */

/* ── Geometrija mikrofona — tetraedarski raspored ─────────────────────────── */
/* Brid tetraedra = 10 cm; M1 u ishodištu koordinatnog sustava.               */
/*                                                                              */
/*   M1 (0,0,0) ─── M2 (10cm, 0, 0)                                           */
/*        │                                                                     */
/*   M3 (5cm, 8.66cm, 0)   M4 (5cm, 2.89cm, 8.16cm)                          */
/*                                                                              */
/* ADC redosljed: RANK1=M1(PA0), RANK2=M2(PA1),  RANK3=M3(PC0), RANK4=M4(PC1)*/
#define MIC_TETRA_EDGE        0.10f
#define MIC_DIST_M            MIC_TETRA_EDGE  /* za 2D lokalizaciju (M1-M2)  */

#define MIC1_X  0.0f
#define MIC1_Y  0.0f
#define MIC1_Z  0.0f

#define MIC2_X  MIC_TETRA_EDGE
#define MIC2_Y  0.0f
#define MIC2_Z  0.0f

#define MIC3_X  (MIC_TETRA_EDGE * 0.5f)
#define MIC3_Y  (MIC_TETRA_EDGE * 0.86602540378f)
#define MIC3_Z  0.0f

#define MIC4_X  (MIC_TETRA_EDGE * 0.5f)
#define MIC4_Y  (MIC_TETRA_EDGE * 0.28867513459f)
#define MIC4_Z  (MIC_TETRA_EDGE * 0.81649658092f)

/* ── TDOA ograničenje ─────────────────────────────────────────────────────── */
/* Max akustički TDOA za brid 10 cm: 0.10/343 * 64000 = 18.66 uzoraka → 20  */
#define TDOA_MAX_SAMPLES      20

/* ── Slanje podataka ──────────────────────────────────────────────────────── */
#define SEND_AUDIO_FRAMES     0

#endif /* AUDIO_COMMON_H */
