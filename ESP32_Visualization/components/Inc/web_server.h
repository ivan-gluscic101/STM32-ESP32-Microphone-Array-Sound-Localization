#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>

/**
 * @brief Pokreće HTTP server i WebSocket endpoint.
 */
void web_server_init(void);

/**
 * @brief Šalje podatke o kutu svim spojenim WebSocket klijentima.
 *        azimuth — kut u XZ ravnini (-90°..+90°, 0 = ispred mic-axisa),
 *        polar — visinski kut (0 = XZ ravnina, trenutno uvijek 0).
 */
void web_server_send_data(float azimuth, float polar, uint8_t strength);

#endif