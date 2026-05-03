/*
 * audio_common.h — Zajedničke konstante za audio sustav.
 *
 * ZAŠTO OVA DATOTEKA POSTOJI:
 *   Originalno su NUM_CH, HALF_SIZE, SAMPLES_PER_CHANNEL bili definirani
 *   na 4 različita mjesta (main.c, TwoDimSoundLoc.h, EMA_FILTER.h, SMA_FILTER.h)
 *   s NEKONZISTENTNIM vrijednostima:
 *     - main.c:           SAMPLES_PER_CHANNEL = 512
 *     - TwoDimSoundLoc.h: HALF_SIZE = 256
 *
 *   To je značilo da LOC_Process obrađuje samo 256 od 512 uzoraka
 *   u half-bufferu — gubitak 50% podataka.
 *
 *   Sada je SVE na jednom mjestu. Promijeniš ovdje → promijeni se svugdje.
 */

#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

/* ── Broj kanala (mikrofona) ──────────────────────────────────────────────── */
#define NUM_CH                4

/* ── Uzorci po kanalu u jednom half-bufferu ────────────────────────────────── */
/*    MORA se podudarati s DMA konfiguracijom:
 *    FULL_BUFFER = NUM_CH * SAMPLES_PER_CHANNEL * 2
 *    DMA half-transfer interrupt se okida na FULL_BUFFER / 2 = HALF_BUFFER
 */
#define SAMPLES_PER_CHANNEL   512
#define HALF_SIZE             SAMPLES_PER_CHANNEL   /* alias za LOC_Process */

/* ── Izvedene veličine buffera ────────────────────────────────────────────── */
#define HALF_BUFFER           (NUM_CH * SAMPLES_PER_CHANNEL)   /* 2048 elemenata */
#define FULL_BUFFER           (HALF_BUFFER * 2)                /* 4096 elemenata */

/* ── Frekvencija uzorkovanja ──────────────────────────────────────────────── */
/* TIM8 ARR=2655 → 64 kHz trigger; ADC scan mode (DISCONT disabled) →
 * svaki trigger konvertira sva 4 kanala → 64 kHz po kanalu.              */
#define SAMPLE_RATE_HZ        64000
#define SAMPLE_PERIOD_S       (1.0f / SAMPLE_RATE_HZ)   /* 15.625 µs */

/* ── Fizikalne konstante ──────────────────────────────────────────────────── */
#define SPEED_OF_SOUND        343.0f    /* m/s pri sobnoj temperaturi        */
#define MIC_DIST_M            0.20f     /* razmak između mikrofona [m]       */
#define CH_DELAY_S            0.9e-6f   /* ADC sekvencijalni channel offset  */

/* ── Izvedene fizikalne granice ───────────────────────────────────────────── */
/* TDOA_MAX = d/c = 0.20/343 = 583 µs = 37.3 uzoraka → koristimo 38       */
#define TDOA_MAX_SAMPLES      38

/* ── Upravljanje slanjem podataka ─────────────────────────────────────────── */
/* 1 = šalji sirove audio podatke (valnih oblika) putem UART-a               */
/* 0 = šalji samo kut + jakost (uštedi ~4 KB/frame i CPU)                   */
#define SEND_AUDIO_FRAMES     0

#endif /* AUDIO_COMMON_H */