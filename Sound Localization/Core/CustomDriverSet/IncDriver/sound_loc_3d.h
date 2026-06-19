#ifndef SOUND_LOC_3D_H
#define SOUND_LOC_3D_H

/*
 * sound_loc_3d.h — 3D lokalizacija zvuka sa 4 mikrofona
 *
 * Algoritam:
 *   1. GCC-PHAT za 3 para: M1-M2, M1-M3, M1-M4
 *   2. Korekcija ADC sekvencijalnog offseta (870.6 ns po kanalu)
 *   3. u = M_geom · tau  (M_geom = c·inv(D), računato u LOC3D_Init iz pozicija)
 *   4. Smjer PREMA izvoru s = −u; Azimut = atan2(sy, sx), Elevacija = asin(sz)
 *
 * Koordinatni sustav (pozicije u audio_common.h), azimut = atan2(sy,sx)
 * wrapan u puni krug [0, 360).
 * Izlaz az_tenth je u rasponu [0, 3600] tenths (tj. [0°, 360°)):
 *   +X →    0°  naprijed (bisektrisa M2/M3)   +Y →   90°  lijevo (M2)
 *   −X →  180°  nazad (M1)                     −Y →  270°  desno (M3)
 *   +Z → elevacija +90° (gore, M4). M1 je u ishodištu (referentni mik).
 *
 * Elevacija el_tenth ostaje fizički [-900, +900] tenths ([-90°, +90°]) —
 * ne wrapa se u 360 jer elevacija po definiciji ne prelazi ±90°.
 */

#include "audio_common.h"
#include <stdint.h>

/* Izračunaj M_geom iz pozicija mikrofona (audio_common.h). Pozvati JEDNOM na
 * startu prije prvog LOC3D_Process (npr. uz GCC_Init). */
void LOC3D_Init(void);

typedef struct {
    int16_t  az_tenth;   /* azimut u 0.1°,  raspon 0..3600 (0°..360°)  */
    int16_t  el_tenth;   /* elevacija u 0.1°, raspon -900..+900 (±90°)  */
    uint8_t  strength;   /* jakost signala 0-100                        */
    uint8_t  _pad;
} loc3d_result_t;


/*
 * LOC3D_Process — interno pronađe energetski vrh (find_peak_offset) unutar
 * sliding buffera, centrira SAMPLES_PER_CHANNEL prozor na njega i vrati 3D smjer.
 *
 * @param buf   interleaved [s*NUM_CH + ch] uint16_t sliding buffer (read-only),
 *              duljine 2*SAMPLES_PER_CHANNEL frejmova
 * @param out   rezultat lokalizacije (validan samo ako je povrat 1)
 * @return      1 = valjan smjer izračunat, 0 = tišina / loša korelacija / cooldown
 */
uint8_t LOC3D_Process(const uint16_t *buf, loc3d_result_t *out);

#endif /* SOUND_LOC_3D_H */
