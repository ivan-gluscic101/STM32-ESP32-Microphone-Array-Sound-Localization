#ifndef SOUND_LOC3D_4MIC_TIME_H
#define SOUND_LOC3D_4MIC_TIME_H

/*
 * sound_loc3d_4mic_time.h — lokalizacija zvuka sa 4 mikrofona (M1..M4) u
 * VREMENSKOJ domeni, bez FFT (frekvencijske domene).
 *
 * Analogno loc3d_3mic_time.c, ali koristi i M4 (RANK4) kao 4. mjerenje za
 * rješavanje punog 3D sustava (3×3). PRETPOSTAVLJA z >= 0 (izvor iznad/u ravnini
 * niza): ako 3×3 sustav vrati negativnu elevaciju (M4 šum), zadrži (sx,sy) iz
 * horizontalnog rješenja i rekonstruira sz >= 0 iz |s|=1 — kao 3-mic verzija.
 * M4 time služi kao dodatna provjera, ne forsira elevaciju ispod horizonta.
 *
 * Princip (time-of-arrival pragom, bez FFT, bez micanja DC-a):
 *   1. DC je fiksno DC_LEVEL. Uzorak "okida" kad |sample − DC| > THRESHOLD_LEVEL.
 *   2. Za svaki kanal (M1..M4) bilježi se PRVI uzorak u kojem okine (onset).
 *   3. TDOA za geometriju relativno na M1:
 *        tau1j = (onset_Mj − onset_M1) · Ts,   j = 2,3,4
 *      Korigira se ADC sekvencijalni offset (CH_DELAY_S po RANK-u).
 *   4. Smjer: u = M_geom · tau   (M_geom = c·inv(D), D = baseline vektori Mj−M1).
 *      Smjer PREMA izvoru s = −u, normaliziran. Ako sz < 0 → rekonstrukcija uz z>=0.
 *   5. Azimut = atan2(sy, sx) wrap [0,360);  Elevacija = asin(sz) ∈ [0,+90].
 *
 * Geometrija (audio_common.h): pravilan tetraedar, brid 13 cm, M4 vrh na 10.61 cm.
 *
 * Koordinatni sustav identičan ostalim verzijama:
 *   +X →   0°  naprijed (bisektrisa M2/M3)   +Y →  90°  lijevo (M2)
 *   −X → 180°  nazad                          −Y → 270°  desno (M3)
 *   +Z → elevacija prema gore (M4)
 */

#include "audio_common.h"
#include <stdint.h>

/* Izračunaj M_geom (3x3) iz pozicija M1..M4 (audio_common.h). Pozvati JEDNOM
 * na startu prije prvog LOC3D_4MIC_TIME_Process. */
void LOC3D_4MIC_TIME_Init(void);

typedef struct {
    int16_t  az_tenth;   /* azimut u 0.1°,  raspon 0..3600 (0°..360°)    */
    int16_t  el_tenth;   /* elevacija u 0.1°, raspon 0..+900 (0°..+90°)  */
    uint8_t  strength;   /* jakost signala 0-100                         */
    uint8_t  _pad;
} loc3d_4mic_time_result_t;

/*
 * LOC3D_4MIC_TIME_Process — time-domain 3D lokalizacija iz 4 mikrofona.
 *
 * @param buf   interleaved [s*NUM_CH + ch] uint16_t sliding buffer (read-only),
 *              duljine 2*SAMPLES_PER_CHANNEL frejmova.
 * @param out   rezultat lokalizacije (validan samo ako je povrat 1)
 * @return      1 = valjan smjer izračunat, 0 = tišina / loš onset / cooldown
 */
uint8_t LOC3D_4MIC_TIME_Process(const uint16_t *buf, loc3d_4mic_time_result_t *out);

#endif /* SOUND_LOC3D_4MIC_TIME_H */
