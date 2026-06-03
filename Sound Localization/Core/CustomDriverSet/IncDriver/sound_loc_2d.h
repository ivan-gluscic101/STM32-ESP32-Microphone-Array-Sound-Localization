#ifndef SOUND_LOC_2D_H
#define SOUND_LOC_2D_H

/*
 * sound_loc_2d.h — 2D lokalizacija zvuka s 3 mikrofona.
 *
 * Geometrija: jednakostraničan trokut, brid d = 10 cm.
 *
 *       M3 (0.05, 0.0866)
 *      / \
 *     /   \
 *   M1 --- M2
 * (0,0)  (0.10, 0)
 *
 * Kanali:  M1 = RANK1 (PB14), M2 = RANK2 (PC0), M3 = RANK3 (PC1)
 *
 * Algoritam (far-field plane wave):
 *   τ12 × c = M2x × cos θ
 *   τ13 × c = M3x × cos θ + M3y × sin θ
 *
 *   ux = τ12 / M2x
 *   uy = (τ13 - (M3x/M2x) × τ12) / M3y
 *   θ  = atan2(uy, ux)
 *
 * TDOA: time-domain cross-correlation (NCC_FindTDOA) + CH_DELAY korekcija.
 * Elevacija se postavlja na 0 u rezultatu.
 */

#include "audio_common.h"
#include "sound_loc_3d.h"   /* loc3d_result_t — isti tip, el_tenth = 0 */
#include <stdint.h>

/*
 * LOC2D_Process — obradi cijeli sliding_buf, automatski pronađi energetski vrh,
 * izračunaj azimutni kut iz M1/M2/M3 i popuni out.
 *
 * @param buf   interleaved [s*NUM_CH + ch] uint16_t sliding buffer (read-only)
 * @param out   az_tenth: azimut u 0.1°, el_tenth: uvijek 0, strength: 0-100
 * @return      1 = valjan rezultat, 0 = tišina / loša korelacija / cooldown
 */
uint8_t LOC2D_Process(const uint16_t *buf, loc3d_result_t *out);

#endif /* SOUND_LOC_2D_H */
