#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

/* ── Buffer layout ─────────────────────────────────────────────────────────── */
#define NUM_CH                4
#define SAMPLES_PER_CHANNEL   1024
#define HALF_SIZE             SAMPLES_PER_CHANNEL
#define HALF_BUFFER           (NUM_CH * SAMPLES_PER_CHANNEL)   /* 4096 uzoraka */
#define FULL_BUFFER           (HALF_BUFFER * 2)                /* 8192 uzoraka */

/* ── Build konfiguracija ──────────────────────────────────────────────────────
 * 1 = ACQ_Task koristi sintetičke pljeskove iz mock_adc (bez mikrofona); to
 *     alocira ~68 KB tablica u BSS. 0 = pravi ADC podaci (mock se ne kompajlira). */
#define USE_MOCK_ADC          0

/* ── Lokalizacijski mod ────────────────────────────────────────────────────────
 * 1 = 3-mikrofonska lokalizacija (M1,M2,M3; M4 se uzorkuje ali se NE koristi).
 *     Koristi loc3d_3mic.c — elevacija uz pretpostavku z >= 0. Uključi dok je
 *     4. mikrofon (RANK4, PC2) neispravan.
 * 0 = puna 4-mikrofonska 3D lokalizacija (sound_loc_3d.c). */
#define USE_3MIC_LOC          1

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

/* ── Geometrija mikrofona ─────────────────────────────────────────────────────
 * Izmjerene pozicije fizičkog niza (vrijednosti u cm → m). M1 je u ishodištu
 * (referentni mikrofon za TDOA). Baza M1-M2-M3 je ~jednakostraničan trokut
 * (bridovi ~10 cm), M4 je vrh iznad baze.
 *
 *   M1 = ( 0.00,  0.00, 0.00) cm   RANK1, PB14
 *   M2 = ( 8.67,  5.00, 0.00) cm   RANK2, PC0
 *   M3 = ( 8.67, −5.00, 0.00) cm   RANK3, PC1
 *   M4 = ( 5.00,  0.00, 8.00) cm   RANK4, PC2   (vrh)
 *
 * Azimut = atan2(y, x); elevacija = asin(z). Izlaz az_tenth ∈ [-1800,+1800]
 * tenths, tj. raspon [-180°, +180°] (NE 0-360°):
 *   +X →    0°  naprijed (bisektrisa između M2 i M3)
 *   +Y →  +90°  M2 strana (lijevo)
 *   −Y →  −90°  M3 strana (desno)
 *   −X → ±180°  M1 strana (nazad)
 *   +Z → elevacija +90°  gore (M4)
 *
 * M_geom se računa iz ovih pozicija u LOC3D_Init() (runtime), pa promjena
 * koordinata automatski propagira — ništa drugo ne treba ručno mijenjati. */
#define MIC1_X   0.0000f
#define MIC1_Y   0.0000f
#define MIC1_Z   0.0000f

#define MIC2_X   0.0867f   /*  8.67 cm */
#define MIC2_Y   0.0500f   /*  5.00 cm */
#define MIC2_Z   0.0000f

#define MIC3_X   0.0867f   /*  8.67 cm */
#define MIC3_Y  (-0.0500f) /* −5.00 cm */
#define MIC3_Z   0.0000f

#define MIC4_X   0.0500f   /*  5.00 cm */
#define MIC4_Y   0.0000f
#define MIC4_Z   0.0800f   /*  8.00 cm */

/* ── TDOA ograničenje ─────────────────────────────────────────────────────────
 * Korelira se M1 vs M2/M3/M4; najveći baseline od M1 ≈ 10 cm (M1-M2, M1-M3).
 * 0.10/343 · 64000 = 18.66 uzoraka → 20. Povećaj ako proširiš niz. */
#define TDOA_MAX_SAMPLES      20

#endif /* AUDIO_COMMON_H */
