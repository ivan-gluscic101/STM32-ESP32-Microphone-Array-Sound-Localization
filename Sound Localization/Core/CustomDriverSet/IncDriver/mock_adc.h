#ifndef MOCK_ADC_H
#define MOCK_ADC_H

/*
 * mock_adc — sintetički ADC generator za testiranje 3D lokalizacije.
 *
 * Generira interleaved [M1,M2,M3,M4,...] buffer koji točno reflektira:
 *   - 12-bit ADC raspon (0-4095), DC offset 2048
 *   - SAMPLE_RATE_HZ = 64 kHz (perioda 15.625 µs)
 *   - CH_DELAY_S = 352.9 ns sekvencijalni offset između kanala
 *   - Akustika: TDOA = -dir · pozicija_mikrofona / c (343 m/s)
 *   - Gaussov burst (~0.8 ms) modulran bijelim šumom — simulira pljesak
 *
 * Tablica MOCK_NUM_DIRS pljeskova precomputed u Mock_Init() (BSS, ~32 KB).
 * U runtime Mock_FillHalf() radi samo memcpy — bez float matematike u
 * ACQ_Task hot pathu.
 *
 * Sekvenca:
 *   event 0:  burst smjer 0
 *   event 1..23: tišina + šum
 *   event 24: burst smjer 1
 *   ...
 *   event 192: burst smjer 0  (wraparound)
 */

#include "audio_common.h"
#include <stdint.h>

#define MOCK_NUM_DIRS         8

/* Pretkalkulira sve pljeskove. Mora se pozvati JEDNOM prije prvog Mock_FillHalf. */
void Mock_Init(void);

/* Puni jedan half-buffer (HALF_BUFFER = 4096 uint16_t) sintetičkim podacima.
 * Interno održava brojač eventa pa svaki sljedeći poziv vraća sljedeći frame. */
void Mock_FillHalf(uint16_t *dst);

/* Debug: indeks zadnjeg burstanog smjera (0..MOCK_NUM_DIRS-1). Čitaj u
 * sound_loc_3d.c na breakpointu da znaš koji smjer je trigerirao detekciju. */
extern volatile uint32_t g_mock_dir_idx;

/* Debug: brojač DMA evenata (raste pri svakom Mock_FillHalf pozivu). */
extern volatile uint32_t g_mock_event_count;

#endif /* MOCK_ADC_H */
