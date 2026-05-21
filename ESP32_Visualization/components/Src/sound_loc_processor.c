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
 * State machine za parsiranje angle paketa — podrška za 2D i 3D
 * Format 2D (Type 0x02): [0xAA][0xBB][0x02][PHI_H][PHI_L][STR][0xCC][0xDD]
 * Format 3D (Type 0x03): [0xAA][0xBB][0x03][AZ_H][AZ_L][EL_H][EL_L][STR][0xCC][0xDD]
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
    S_EOF1,
    S_EOF2,
} pkt_state_t;

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
                    state = (b == 0x02 || b == 0x03) ? S_AZ_H : S_SOF1;
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
                case S_EOF1:
                    state = (b == ANGLE_PKT_EOF1) ? S_EOF2 : S_SOF1;
                    break;
                case S_EOF2:
                    if (b == ANGLE_PKT_EOF2) {
                        int16_t az_tenth = (int16_t)((az_h << 8) | az_l);
                        int16_t el_tenth = (pkt_type == 0x03) ? (int16_t)((el_h << 8) | el_l) : 0;
                        float azimuth = az_tenth / 10.0f;
                        float elevation = el_tenth / 10.0f;
                        ESP_LOGI(TAG, "Type %d | Az: %.1f° | El: %.1f° | Strength: %d", pkt_type, azimuth, elevation, str_val);
                        web_server_send_data(azimuth, elevation, str_val);
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