#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>

/**
 * @brief Pokreće HTTP server i WebSocket endpoint.
 */
void web_server_init(void);

/**
 * @brief Šalje podatke o kutu svim spojenim WebSocket klijentima.
 *        Koordinatni sustav je isti kao u glavnom projektu (Sound Localization):
 *        azimuth — kut u XY ravnini [0°, 360°), 0° = +X naprijed, 90° = +Y lijevo,
 *                  180° = nazad, 270° = desno.
 *        polar   — elevacija [-90°, +90°], +90° = ravno gore (+Z), 0 = horizont.
 *                  (Naziv parametra je povijesni; vrijednost je elevacija.)
 */
void web_server_send_data(float azimuth, float polar, uint8_t strength);

#endif