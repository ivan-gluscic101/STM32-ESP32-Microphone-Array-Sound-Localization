#ifndef MICROPHONE_CONFIG_H
#define MICROPHONE_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file microphone_config.h
 * @brief Konfiguracija lokacija mikrofona kao matrica
 * 
 * Geometrija mikrofona — pozicije u cm. MORA odgovarati STM32 strani
 * (Core/CustomDriverSet/IncDriver/audio_common.h, MIC*_{X,Y,Z} u metrima);
 * ovdje se koristi SAMO za 3D vizualizaciju u web pregledniku (crtanje
 * mikrofona), ne za izračun — STM32 računa azimut/elevaciju.
 *
 * M1, M2, M3 = jednakostranični trokut, stranica 13 cm, u ravnini z=0.
 * M1 u ishodištu; M2/M3 simetrični oko X-osi (M2 +Y lijevo, M3 −Y desno),
 * bisektrisa M2/M3 pada na +X. M4 je vrh iznad baze (zasad se ne koristi).
 *
 * Koordinatni sustav:
 *   +X → 0°  naprijed (bisektrisa M2/M3)
 *   +Y → +90° lijevo (M2)
 *   −X → 180° nazad (M1 strana)
 *   −Y → 270° desno (M3)
 *   +Z → elevacija +90° (gore, M4)
 */

/* Broj mikrofona */
#define NUM_MICROPHONES 4

/* Struktura za mikrofon */
typedef struct {
    float x;          /* pozicija X [cm] */
    float y;          /* pozicija Y [cm] */
    float z;          /* pozicija Z [cm] */
    const char *name; /* označava, npr. "M1", "M2", ... */
} microphone_t;

/* Matrica lokacija mikrofona — usklađena s audio_common.h (cm = m × 100).
 * Jednakostranični trokut a=13cm: x = a·√3/2 = 11.2583, y = a/2 = 6.5 cm. */
static const microphone_t microphones[NUM_MICROPHONES] = {
    { .x =  0.0000f, .y =  0.00f, .z = 0.00f, .name = "M1" },  /* referentni */
    { .x = 11.2583f, .y =  6.50f, .z = 0.00f, .name = "M2" },  /* lijevo (+Y) */
    { .x = 11.2583f, .y = -6.50f, .z = 0.00f, .name = "M3" },  /* desno (−Y) */
    { .x =  0.0000f, .y =  0.00f, .z = 8.00f, .name = "M4" }   /* vrh */
};

/**
 * @brief Dohvati konfiguraciju mikrofona
 * @return Pokazivač na niz mikrofona
 */
static inline const microphone_t* microphone_config_get_all(void) {
    return microphones;
}

/**
 * @brief Dohvati broj mikrofona
 * @return Broj mikrofona
 */
static inline int microphone_config_get_count(void) {
    return NUM_MICROPHONES;
}

/**
 * @brief Dohvati pojedini mikrofon
 * @param index Indeks mikrofona (0-3)
 * @return Pokazivač na strukturu mikrofona ili NULL ako indeks nije validan
 */
static inline const microphone_t* microphone_config_get(int index) {
    if (index < 0 || index >= NUM_MICROPHONES) {
        return NULL;
    }
    return &microphones[index];
}

#endif /* MICROPHONE_CONFIG_H */
