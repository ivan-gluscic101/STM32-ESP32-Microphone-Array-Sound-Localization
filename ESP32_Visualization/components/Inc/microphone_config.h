#ifndef MICROPHONE_CONFIG_H
#define MICROPHONE_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file microphone_config.h
 * @brief Konfiguracija lokacija mikrofona kao matrica
 * 
 * Geometrija mikrofona — pozicije u cm (pretvaraju se u m prema potrebi).
 * M1 je u ishodištu (referentni mikrofon za TDOA).
 * Baza M1-M2-M3 je ~jednakostraničan trokut (~10 cm bridovi).
 * M4 je vrh iznad baze (mjerenje elevacije).
 * 
 * Koordinatni sustav:
 *   +X → 0°  naprijed (bisektrisa M2/M3)
 *   +Y → +90° lijevo (M2)
 *   −Y → −90° desno (M3)
 *   −X → ±180° nazad (M1)
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

/* Matrica lokacija mikrofona — lagano mijenjiva konfiguracija */
static const microphone_t microphones[NUM_MICROPHONES] = {
    { .x =  0.00f, .y =  0.00f, .z =  0.00f, .name = "M1" },  /* referentni */
    { .x =  8.67f, .y =  5.00f, .z =  0.00f, .name = "M2" },  /* lijevo */
    { .x =  8.67f, .y = -5.00f, .z =  0.00f, .name = "M3" },  /* desno */
    { .x =  5.00f, .y =  0.00f, .z =  8.00f, .name = "M4" }   /* vrh */
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
