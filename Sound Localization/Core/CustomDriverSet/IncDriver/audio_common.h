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
 * Sve verzije rade TIME-domain onset TDOA (prag na amplitudi po kanalu, TDOA iz
 * razmaka onseta). FFT/GCC-PHAT verzije su uklonjene iz projekta.
 *
 * 1 = puna 4-mikrofonska TIME-domain lokalizacija (sound_loc3d_4mic_time.c).
 *     Koristi sva 4 mikrofona (pravilan tetraedar) → pravi predznak elevacije,
 *     bez pretpostavke z >= 0.
 * 0 = 3-mikrofonska TIME-domain lokalizacija (loc3d_3mic_time.c): M1,M2,M3;
 *     M4 se uzorkuje ali se NE koristi. Elevacija uz pretpostavku z >= 0.
 *     Koristi dok je 4. mikrofon (RANK4, PC2) neispravan. */
#define USE_4MIC_TIME_LOC     0

/* Zadržano radi kompatibilnosti konfiguracije: jedina podržana vrijednost je 1
 * (frekvencijska/GCC-PHAT domena više ne postoji). task_manager.c će prijaviti
 * #error ako je postavljeno na 0. */
#define USE_TIME_DOMAIN_LOC   1

/* ── Vremenski parametri ───────────────────────────────────────────────────── */
/* TIM8 ARR=884 @ 170 MHz → okida ADC svakih 5.21 µs = 192.09 kHz po kanalu.
 * Viša fs = sitniji uzorak = finija kutna rezolucija (time-domain TDOA ovisi
 * linearno o fs). ADC scan (4×870.6 ns = 3.48 µs) i dalje stane u 5.21 µs. */
#define SAMPLE_RATE_HZ        192000
#define SAMPLE_PERIOD_S       (1.0f / SAMPLE_RATE_HZ)          /* 5.208 µs    */

/* ── ADC sekvencijalni offset ─────────────────────────────────────────────── */
/* ADC clock = PCLK/4 = 170 MHz/4 = 42.5 MHz → perioda 23.53 ns. Ovisi o ADC
 * clocku, NE o fs — ostaje isti pri promjeni SAMPLE_RATE_HZ.                  */
/* Svaki kanal = 24.5 (sampling) + 12.5 (conv) = 37 ciklusa = 870.6 ns       */
#define CH_DELAY_S            870.6e-9f

/* ── Akustika ──────────────────────────────────────────────────────────────── */
#define SPEED_OF_SOUND        343.0f    /* m/s pri ~20°C */

/* ── Geometrija mikrofona ─────────────────────────────────────────────────────
 * M1, M2, M3 čine JEDNAKOSTRANIČNI trokut sa stranicom a = 13 cm, u ravnini z=0.
 * M1 je u ishodištu (referentni vrh). M2 i M3 su druga dva vrha, SIMETRIČNA oko
 * X-osi (M2 na +Y = lijevo, M3 na −Y = desno), pa bisektrisa M2/M3 pada na +X.
 *
 * Izvod (M1 u ishodištu, M2/M3 simetrični oko X):
 *   |M2−M3| = 2·y = a            → y = a/2          = 0.065 m
 *   |M1−M2| = sqrt(x² + y²) = a   → x = a·sqrt(3)/2  = 0.112583 m
 *
 *   M1 = ( 0.000000,  0.000000,  0.000000) m   RANK1   referentni
 *   M2 = ( 0.112583, +0.065000,  0.000000) m   RANK2   lijevo  (+Y)
 *   M3 = ( 0.112583, −0.065000,  0.000000) m   RANK3   desno   (−Y)
 *   M4 = ( 0.075055,  0.000000,  0.106145) m   RANK4   vrh tetraedra
 *
 * PRAVILAN TETRAEDAR: sva 4 mikrofona međusobno udaljena a = 13 cm.
 *   M4 leži iznad CENTROIDA baze (cx = (0+0.112583+0.112583)/3 = 0.075055,
 *   cy = 0) na visini h = a·√(2/3) = 0.13·0.816497 = 0.106145 m (10.61 cm).
 *   Provjera: |M1−M4| = |M2−M4| = |M3−M4| = 0.13 m. ✓
 *
 * Azimut = atan2(y, x), wrap [0,360):
 *   0°   → +X  (naprijed, bisektrisa M2/M3)
 *   90°  → +Y  (lijevo, M2)
 *   180° → −X  (nazad, M1 strana)
 *   270° → −Y  (desno, M3)
 * Elevacija: +Z → gore (M4) */
#define MIC1_X   0.000000f
#define MIC1_Y   0.000000f
#define MIC1_Z   0.000000f

#define MIC2_X   0.112583f  /* a·√3/2  (naprijed) */
#define MIC2_Y   0.065000f  /* +a/2    (lijevo)   */
#define MIC2_Z   0.000000f

#define MIC3_X   0.112583f  /* a·√3/2  (naprijed) */
#define MIC3_Y  (-0.065000f) /* −a/2   (desno)    */
#define MIC3_Z   0.000000f

#define MIC4_X   0.075055f  /* centroid baze X (a/√3) */
#define MIC4_Y   0.000000f  /* centroid baze Y        */
#define MIC4_Z   0.106145f  /* h = a·√(2/3) = 10.61 cm (vrh tetraedra) */

/* ── TDOA ograničenje ─────────────────────────────────────────────────────────
 * Korelira se M1 vs M2/M3/M4; u pravilnom tetraedru su SVI bridovi (uklj. M1-M4)
 * jednaki = a = 13 cm, pa je najveći baseline od M1 i dalje 13 cm.
 * 0.13/343 · 192000 = 72.8 uzoraka → 74. Skalira s fs — uskladi pri promjeni
 * SAMPLE_RATE_HZ (≈ baseline/343 · fs). */
#define TDOA_MAX_SAMPLES      74

#endif /* AUDIO_COMMON_H */
