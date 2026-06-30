#ifndef LOC3D_3MIC_TIME_H
#define LOC3D_3MIC_TIME_H

/*
 * loc3d_3mic_time.h — lokalizacija zvuka s 3 mikrofona (M1, M2, M3) u
 * VREMENSKOJ domeni, bez FFT (frekvencijske domene).
 *
 * Princip (time-of-arrival pragom, bez FFT, bez micanja DC-a):
 *   1. DC je fiksno DC_LEVEL (2048). Uzorak "okida" kad |sample − DC| > THR (250).
 *   2. Idemo redom kroz uzorke od početka buffera; za svaki kanal (M1/M2/M3)
 *      bilježimo PRVI uzorak u kojem okine. ref_mic = onaj koji okine prvi.
 *   3. TDOA za geometriju je uvijek relativan na M1:
 *      tau12 = (onset_M2 − onset_M1) · Ts,  tau13 = (onset_M3 − onset_M1) · Ts.
 *      Korigira se ADC sekvencijalni offset (CH_DELAY_S po kanalu) kao i drugdje.
 *   4. Smjer u ravnini: [sx; sy] = −M_geom2 · [tau12; tau13]  (M_geom2 = c·inv(D2)
 *      iz pozicija M1/M2/M3). Elevacija sz uz pretpostavku z >= 0.
 *   5. Azimut = atan2(sy, sx) wrap [0,360);  Elevacija = asin(sz) ∈ [0,+90].
 *
 * DC_LEVEL i THRESHOLD_LEVEL su #define u loc3d_3mic_time.c (lako podesivi).
 *
 * M4 (ch3) se uzorkuje ali se NE koristi (neispravan kanal).
 *
 * Koordinatni sustav identičan ostalim verzijama (audio_common.h):
 *   +X →   0°  naprijed (bisektrisa M2/M3)   +Y →  90°  lijevo (M2)
 *   −X → 180°  nazad                          −Y → 270°  desno (M3)
 *   +Z → elevacija prema gore (pretpostavljeno >= 0)
 */

#include "audio_common.h"
#include <stdint.h>

/* Izračunaj M_geom2 (2x2) iz pozicija M1/M2/M3 (audio_common.h). Pozvati JEDNOM
 * na startu prije prvog LOC3D_3MIC_TIME_Process. */
void LOC3D_3MIC_TIME_Init(void);

typedef struct {
    int16_t  az_tenth;   /* azimut u 0.1°,  raspon 0..3600 (0°..360°)  */
    int16_t  el_tenth;   /* elevacija u 0.1°, raspon 0..+900 (0°..+90°) */
    uint8_t  strength;   /* jakost signala 0-100                        */
    uint8_t  _pad;
} loc3d_3mic_time_result_t;

/*
 * LOC3D_3MIC_TIME_Process — time-domain lokalizacija iz 3 mikrofona.
 *
 * @param buf   interleaved [s*NUM_CH + ch] uint16_t sliding buffer (read-only),
 *              duljine 2*SAMPLES_PER_CHANNEL frejmova.
 * @param out   rezultat lokalizacije (validan samo ako je povrat 1)
 * @return      1 = valjan smjer izračunat, 0 = tišina / loš onset / cooldown
 */
uint8_t LOC3D_3MIC_TIME_Process(const uint16_t *buf, loc3d_3mic_time_result_t *out);

#endif /* LOC3D_3MIC_TIME_H */
