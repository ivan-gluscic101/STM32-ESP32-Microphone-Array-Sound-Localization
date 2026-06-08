#include "sound_loc_processor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "uart_driver.h"
#include "web_server.h"

static const char *TAG = "SOUND_LOC_PROC";

/*
 * State machine za parsiranje angle paketa — podrška za 2D, 3D i RAW capture
 * Format 2D  (Type 0x02): [0xAA][0xBB][0x02][PHI_H][PHI_L][STR][0xCC][0xDD]
 * Format 3D  (Type 0x03): [0xAA][0xBB][0x03][AZ_H][AZ_L][EL_H][EL_L][STR][0xCC][0xDD]
 * Format RAW (Type 0x04): [0xAA][0xBB][0x04][NCH][N_H][N_L]
 *                          + NCH*N uzoraka (big-endian uint16, kanal-major)
 *                          + [0xCC][0xDD]
 */
typedef enum {
    S_SOF1 = 0,
    S_SOF2,
    S_TYPE,
    S_AZ_H,
    S_AZ_L,
    S_EL_H,
    S_EL_L,
    S_STR,
    S_RAW_NCH,
    S_RAW_NH,
    S_RAW_NL,
    S_RAW_BODY,
    S_EOF1,
    S_EOF2,
} pkt_state_t;

/* Ispiši primljeni sirovi capture kao CSV na ESP32 konzolu (za copy u MATLAB).
 * Layout u raw[]: kanal-major, big-endian uint16 → raw[(ch*n + s)*2]. */
static void raw_capture_print(const uint8_t *raw, uint8_t nch, uint16_t n) {
    printf("RAW START NCH=%u N=%u\n", (unsigned)nch, (unsigned)n);
    for (uint16_t s = 0; s < n; s++) {
        for (uint8_t ch = 0; ch < nch; ch++) {
            size_t   idx = ((size_t)ch * n + s) * 2u;
            uint16_t v   = ((uint16_t)raw[idx] << 8) | raw[idx + 1];
            printf("%u%c", (unsigned)v, (ch + 1 < nch) ? ',' : '\n');
        }
    }
    printf("RAW END\n");
}

#define LINE_BUF_SIZE 160

static void rx_task(void *arg) {
    uint8_t *data = (uint8_t *) malloc(UART_BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Neuspjela alokacija buffera!");
        vTaskDelete(NULL);
        return;
    }

    pkt_state_t state    = S_SOF1;
    uint8_t     pkt_type = 0;
    uint8_t     az_h     = 0;
    uint8_t     az_l     = 0;
    uint8_t     el_h     = 0;
    uint8_t     el_l     = 0;
    uint8_t     str_val  = 0;

    /* RAW capture (Type 0x04) */
    static uint8_t raw_buf[RAW_MAX_BYTES];
    uint8_t        raw_nch   = 0;
    uint16_t       raw_n     = 0;
    size_t         raw_total = 0;   /* nch * n * 2 */
    size_t         raw_idx   = 0;

    static char line_buf[LINE_BUF_SIZE];
    size_t      line_idx = 0;

    int pkt_count = 0;
    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0 && pkt_count == 0) {
            ESP_LOGI(TAG, "UART prima podatke! Broj bajtova: %d", len);
            pkt_count = 1;
        }

        for (int i = 0; i < len; i++) {
            uint8_t b = data[i];

            switch (state) {
                case S_SOF1:
                    if (b == ANGLE_PKT_SOF1) {
                        line_idx = 0;
                        state = S_SOF2;
                    } else if (b == '\n') {
                        line_buf[line_idx] = '\0';
                        if (line_idx > 0) {
                            ESP_LOGI(TAG, "%s", line_buf);
                        }
                        line_idx = 0;
                    } else if (b != '\r' && b >= 0x20 && b <= 0x7E) {
                        if (line_idx < LINE_BUF_SIZE - 1) {
                            line_buf[line_idx++] = (char)b;
                        }
                    }
                    break;
                case S_SOF2:
                    state = (b == ANGLE_PKT_SOF2) ? S_TYPE : S_SOF1;
                    break;
                case S_TYPE:
                    pkt_type = b;
                    if (b == 0x02 || b == 0x03) {
                        state = S_AZ_H;
                    } else if (b == ANGLE_PKT_TYPE_RAW) {
                        state = S_RAW_NCH;
                    } else {
                        state = S_SOF1;
                    }
                    break;
                case S_AZ_H:
                    az_h = b;
                    state = S_AZ_L;
                    break;
                case S_AZ_L:
                    az_l = b;
                    state = (pkt_type == 0x03) ? S_EL_H : S_STR;
                    break;
                case S_EL_H:
                    el_h = b;
                    state = S_EL_L;
                    break;
                case S_EL_L:
                    el_l = b;
                    state = S_STR;
                    break;
                case S_STR:
                    str_val = b;
                    state = S_EOF1;
                    break;
                case S_RAW_NCH:
                    raw_nch = b;
                    state = S_RAW_NH;
                    break;
                case S_RAW_NH:
                    raw_n = (uint16_t)((uint16_t)b << 8);
                    state = S_RAW_NL;
                    break;
                case S_RAW_NL:
                    raw_n |= (uint16_t)b;
                    raw_total = (size_t)raw_nch * raw_n * 2u;
                    raw_idx   = 0;
                    /* Zaštita: ako zaglavlje premaši buffer, odbaci paket. */
                    if (raw_nch == 0 || raw_n == 0 || raw_total > RAW_MAX_BYTES) {
                        ESP_LOGW(TAG, "RAW paket prevelik/nevaljan (NCH=%u N=%u), odbacujem",
                                 (unsigned)raw_nch, (unsigned)raw_n);
                        state = S_SOF1;
                    } else {
                        state = S_RAW_BODY;
                    }
                    break;
                case S_RAW_BODY:
                    raw_buf[raw_idx++] = b;
                    if (raw_idx >= raw_total) {
                        state = S_EOF1;
                    }
                    break;
                case S_EOF1:
                    state = (b == ANGLE_PKT_EOF1) ? S_EOF2 : S_SOF1;
                    break;
                case S_EOF2:
                    if (b == ANGLE_PKT_EOF2) {
                        if (pkt_type == ANGLE_PKT_TYPE_RAW) {
                            raw_capture_print(raw_buf, raw_nch, raw_n);
                        } else {
                            int16_t az_tenth = (int16_t)((az_h << 8) | az_l);
                            int16_t el_tenth = (pkt_type == 0x03) ? (int16_t)((el_h << 8) | el_l) : 0;
                            float azimuth = az_tenth / 10.0f;
                            float elevation = el_tenth / 10.0f;
                            ESP_LOGI(TAG, "Type %d | Az: %.1f° | El: %.1f° | Strength: %d", pkt_type, azimuth, elevation, str_val);
                            web_server_send_data(azimuth, elevation, str_val);
                        }
                    }
                    state = S_SOF1;
                    break;
            }
        }
    }
    free(data);
}

void sound_loc_processor_init(void) {
    ESP_LOGI(TAG, "Inicijalizacija procesora kuta...");
    xTaskCreate(rx_task, "uart_rx_task", 4096, NULL, 10, NULL);
}