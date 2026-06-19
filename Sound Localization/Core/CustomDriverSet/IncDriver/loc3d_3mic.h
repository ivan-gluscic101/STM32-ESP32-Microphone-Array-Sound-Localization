#ifndef LOC3D_3MIC_H
#define LOC3D_3MIC_H

/*
 * loc3d_3mic.h — lokalizacija zvuka s 3 mikrofona (M1, M2, M3); M4 se ignorira.
 *
 * Varijanta sound_loc_3d za slučaj kad 4. mikrofon (kanal RANK4, PC2) ne radi
 * ispravno. M1, M2, M3 leže u ravnini z = 0, pa TDOA daje samo komponente smjera
 * u ravnini (sx, sy). Elevacija se ne može odrediti jednoznačno iz koplanarnog
 * niza — UZ PRETPOSTAVKU z >= 0 računa se sz = sqrt(1 - sx^2 - sy^2).
 *
 * Algoritam:
 *   1. GCC-PHAT za 2 para: M1-M2, M1-M3
 *   2. Korekcija ADC sekvencijalnog offseta (CH_DELAY_S po kanalu)
 *   3. [sx; sy] = -M_geom2 · [tau12; tau13]   (M_geom2 = c·inv(D2), 2x2)
 *   4. sz = +sqrt(max(0, 1 - sx^2 - sy^2))     (pretpostavka: izvor je iznad ravnine)
 *   5. Azimut = atan2(sy, sx) wrapan u [0,360);  Elevacija = asin(sz) u [0, +90]
 *
 * 4. kanal (ch3) se i dalje uzorkuje (ADC sekvenca je fiksna), ali se NE koristi:
 * njegov RMS / GCC-PHAT par se ne računa i ne ulazi ni u jedan gate.
 *
 * Koordinatni sustav identičan glavnoj verziji (audio_common.h):
 *   +X →   0°  naprijed (bisektrisa M2/M3)   +Y →  90°  lijevo (M2)
 *   −X → 180°  nazad                          −Y → 270°  desno (M3)
 *   +Z → elevacija prema gore (pretpostavljeno >= 0)
 */

#include "audio_common.h"
#include <stdint.h>

/* Izračunaj M_geom2 (2x2) iz pozicija M1/M2/M3 (audio_common.h). Pozvati JEDNOM
 * na startu prije prvog LOC3D_3MIC_Process. */
void LOC3D_3MIC_Init(void);

typedef struct {
    int16_t  az_tenth;   /* azimut u 0.1°,  raspon 0..3600 (0°..360°)  */
    int16_t  el_tenth;   /* elevacija u 0.1°, raspon 0..+900 (0°..+90°) */
    uint8_t  strength;   /* jakost signala 0-100                        */
    uint8_t  _pad;
} loc3d_3mic_result_t;

/*
 * LOC3D_3MIC_Process — pronađe energetski vrh, centrira prozor i vrati smjer iz
 * 3 mikrofona (M4 ignoriran).
 *
 * @param buf   interleaved [s*NUM_CH + ch] uint16_t sliding buffer (read-only),
 *              duljine 2*SAMPLES_PER_CHANNEL frejmova. ch3 se čita za snapshot,
 *              ali ne ulazi u izračun smjera.
 * @param out   rezultat lokalizacije (validan samo ako je povrat 1)
 * @return      1 = valjan smjer izračunat, 0 = tišina / loša korelacija / cooldown
 */
uint8_t LOC3D_3MIC_Process(const uint16_t *buf, loc3d_3mic_result_t *out);

#endif /* LOC3D_3MIC_H */
