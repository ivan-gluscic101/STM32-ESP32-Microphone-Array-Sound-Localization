/*
 * TwoDimSoundLoc.h
 *
 * Threshold-based TDOA lokalizacija zvuka s 2 mikrofona.
 * Vraća kut φ ∈ [-90°, +90°] od okomice na os mikrofona.
 *
 * ISPRAVKE:
 *   1. Uklonjene lokalne definicije NUM_CH, HALF_SIZE, SAMPLE_RATE_HZ itd.
 *      Sada se koristi audio_common.h kao jedini izvor istine.
 *   2. HALF_SIZE se sada podudara sa SAMPLES_PER_CHANNEL (512, ne 256).
 *   3. TDOA_MAX_SAMPLES povećan s 9 na 10 (zaokruživanje prema gore).
 */

#ifndef CUSTOMDRIVERSET_INCDRIVER_TWODIMSOUNDLOC_H_
#define CUSTOMDRIVERSET_INCDRIVER_TWODIMSOUNDLOC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <math.h>
#include <stdint.h>

/* Zajedničke konstante — JEDINI IZVOR ISTINE */
#include "audio_common.h"

/* ── Threshold pragovi za detekciju zvuka ──────────────────────────────────── */
/* Idle šum stiže do ~2750 (min) i ~3400 (max) — pragovi moraju biti izvan  */
/* tog raspona da se izbjegnu lažne detekcije.                               */
#define THRESHOLD_HIGH        3600
#define THRESHOLD_LOW         2500

/* ── Cooldown nakon detekcije ──────────────────────────────────────────────── */
/* 1 frame = 1 half-buffer = 512 uzoraka / 64 kHz = 8 ms
 * SILENCE_FRAMES=8 → 64 ms cooldown između detekcija */
#define SILENCE_FRAMES        8

/* ── UART angle paket markeri ──────────────────────────────────────────────── */
#define ANGLE_PKT_SOF1        0xAA
#define ANGLE_PKT_SOF2        0xBB
#define ANGLE_PKT_TYPE        0x02
#define ANGLE_PKT_EOF1        0xCC
#define ANGLE_PKT_EOF2        0xDD

/*
 * LOC_Process — threshold TDOA obrada jednog half-buffera
 *
 * @param buf            pokazivač na početak half-buffera (interleaved, read-only)
 * @param phi_tenth_deg  izlaz: kut u desetinkama stupnjeva kao int16
 * @param strength       izlaz: jakost signala 0–100
 * @return               1 = valjan kut izračunat, 0 = nije detektiran
 */
uint8_t LOC_Process(const uint16_t *buf, int16_t *phi_tenth_deg, uint8_t *strength);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOMDRIVERSET_INCDRIVER_TWODIMSOUNDLOC_H_ */