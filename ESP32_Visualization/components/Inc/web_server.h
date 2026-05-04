#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>

/**
 * @brief Pokreće HTTP server i WebSocket endpoint.
 */
void web_server_init(void);

/**
 * @brief Šalje podatke o kutu svim spojenim WebSocket klijentima.
 */
void web_server_send_data(float angle, uint8_t strength);

#endif