#ifndef SOUND_LOC_3D_H
#define SOUND_LOC_3D_H

/*
 * sound_loc_3d.h — 3D lokalizacija zvuka sa 4 mikrofona (tetraedarski raspored)
 *
 * Algoritam:
 *   1. GCC (normalizirana križna korelacija) za 3 para: M1-M2, M1-M3, M1-M4
 *   2. Korekcija ADC sekvencijalnog offseta (870.6 ns po kanalu)
 *   3. Analitičko rješavanje donjeg trokutnog sustava 3×3 za smjer u = (ux,uy,uz)
 *   4. Azimut = atan2(uy, ux), Elevacija = atan2(uz, sqrt(ux²+uy²))
 *
 * Koordinatni sustav: M1 u ishodištu, os X prema M2, Y gore iz ravnine M1-M2-M3.
 *   θ = 0° → zvuk dolazi od smjera M2 (pozitivna os X)
 *   θ = 90° → zvuk dolazi od smjera M3 (pozitivna os Y)
 *   φ > 0° → zvuk dolazi odozgo (pozitivna os Z)
 */

#include "audio_common.h"
#include <stdint.h>

typedef struct {
    int16_t  az_tenth;   /* azimut u 0.1°,  raspon -1800..+1800 */
    int16_t  el_tenth;   /* elevacija u 0.1°, raspon  -900..+900 */
    uint8_t  strength;   /* jakost signala 0-100                 */
    uint8_t  _pad;
} loc3d_result_t;

/*
 * LOC3D_Process — obradi jedan half-buffer i vrati 3D smjer.
 *
 * @param buf  pokazivač na kopiju half-buffera (read-only, interleaved)
 * @param out  rezultat lokalizacije (samo validan ako je povratna vrijednost 1)
 * @return     1 = valjan smjer izračunat, 0 = tišina / cooldown / nekonzistentni TDOA
 */
uint8_t LOC3D_Process(const uint16_t *buf, loc3d_result_t *out);

#endif /* SOUND_LOC_3D_H */
