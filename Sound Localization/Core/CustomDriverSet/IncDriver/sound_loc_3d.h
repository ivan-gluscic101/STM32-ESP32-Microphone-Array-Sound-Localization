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
 * LOC3D_Process — obradi prozor od SAMPLES_PER_CHANNEL frejmova počevši od
 * `frame_offset` unutar `buf` i vrati 3D smjer.
 *
 * Sliding window: pozivatelj može pozvati funkciju više puta na različitim
 * offsetima unutar istog buffera kako bi uhvatio transient (npr. pljesak) koji
 * leži na granici dva DMA half-buffera.
 *
 * @param buf           interleaved [s*NUM_CH + ch] uint16_t buffer (read-only)
 * @param frame_offset  početni frejm prozora unutar buf
 * @param out           rezultat lokalizacije (validan samo ako je povrat 1)
 * @return              1 = valjan smjer izračunat, 0 = tišina / nekonzistentni TDOA
 */
uint8_t LOC3D_Process(const uint16_t *buf, loc3d_result_t *out);

#endif /* SOUND_LOC_3D_H */
