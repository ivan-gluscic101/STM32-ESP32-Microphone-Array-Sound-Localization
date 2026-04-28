/*
 * TwoDimSoundLoc.h
 * Implementirano: 2026-04-02
 *
 * Threshold-based TDOA lokalizacija zvuka s 2 mikrofona.
 * Vraća kut φ ∈ [-90°, +90°] od okomice na os mikrofona.
 */

#ifndef CUSTOMDRIVERSET_INCDRIVER_TWODIMSOUNDLOC_H_
#define CUSTOMDRIVERSET_INCDRIVER_TWODIMSOUNDLOC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <math.h>
#include <stdint.h>

/* ── Konstante uzorkovanja (moraju odgovarati ADC/TIM konfiguraciji) ────────*/
#define THRESHOLD_HIGH        3550          /* ADC vrijednost iznad koje smatramo da je zvuk */
#define THRESHOLD_LOW    2900
#define HALF_SIZE        256           /* uzoraka po kanalu u jednom half-bufferu        */
#define NUM_CH           4             /* broj interleaved kanala u bufferu              */
#define SAMPLE_RATE_HZ   16000         /* frekvencija uzorkovanja [Hz]                   */
#define SAMPLE_PERIOD_S  0.0000625f    /* 62.5 µs — period jednog uzorka                */

/* ── Fizikalne konstante ────────────────────────────────────────────────────*/
#define SPEED_OF_SOUND   343.0f        /* brzina zvuka pri sobnoj temperaturi [m/s]      */
#define MIC_DIST_M       0.20f         /* razmak između mikrofona [m]                    */
#define CH_DELAY_S       0.0000009f    /* 0.9 µs — ADC sekvencijalni channel offset      */

/* ── Izvedene granice ───────────────────────────────────────────────────────*/
/* Maksimalni fizikalni TDOA = d/c =  583 µs → 9.33 uzorka               */
/* Koristimo 9 kao integer granicu za sanity check                           */
#define TDOA_MAX_SAMPLES 9

/* ── UART angle paket markeri ───────────────────────────────────────────────*/
#define ANGLE_PKT_SOF1   0xAA
#define ANGLE_PKT_SOF2   0xBB
#define ANGLE_PKT_TYPE   0x02          /* razlikuje angle paket od audio framea (0x01)   */
#define ANGLE_PKT_EOF1   0xCC
#define ANGLE_PKT_EOF2   0xDD


#define SILENCE_FRAMES 8 /*Definiramo nakon kojeg vremena prihvaćamo sljedeći zvuk, 1frame = 1halfbuffer -> 256 sampl * 1/16khz = 0.016*/
/*
 * LOC_Process — threshold TDOA obrada jednog half-buffera
 *
 * @param buf            pokazivač na početak half-buffera (interleaved, read-only)
 * @param phi_tenth_deg  izlaz: kut u desetinkama stupnjeva kao int16
 *                       (npr. +254 = +25.4°, -598 = -59.8°)
 * @param strength       izlaz: jakost signala 0–100 (veća = bliže/glasnije)
 * @return               1 = valjan kut izračunat, 0 = trigger nije detektiran
 */
uint8_t LOC_Process(const uint16_t *buf, int16_t *phi_tenth_deg, uint8_t *strength);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOMDRIVERSET_INCDRIVER_TWODIMSOUNDLOC_H_ */
