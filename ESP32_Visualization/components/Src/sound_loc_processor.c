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
 * State machine za parsiranje angle paketa bajt po bajt.
 * Format: [0xAA][0xBB][0x02][PHI_H][PHI_L][STR][0xCC][0xDD]
 * Ispravno sklapa paket čak i kad uart_read_bytes vrati
 * samo dio bajtova u jednom čitanju.
 */
typedef enum {
    S_SOF1 = 0,
    S_SOF2,
    S_TYPE,
    S_PHI_H,
    S_PHI_L,
    S_STR,
    S_EOF1,
    S_EOF2,
} pkt_state_t;

static void rx_task(void *arg) {
    uint8_t *data = (uint8_t *) malloc(UART_BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Neuspjela alokacija buffera!");
        vTaskDelete(NULL);
        return;
    }

    pkt_state_t state   = S_SOF1;
    uint8_t     phi_h   = 0;
    uint8_t     phi_l   = 0;
    uint8_t     str_val = 0;

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);

        for (int i = 0; i < len; i++) {
            uint8_t b = data[i];

            switch (state) {
                case S_SOF1:
                    if (b == ANGLE_PKT_SOF1) state = S_SOF2;
                    break;
                case S_SOF2:
                    state = (b == ANGLE_PKT_SOF2) ? S_TYPE : S_SOF1;
                    break;
                case S_TYPE:
                    state = (b == ANGLE_PKT_TYPE) ? S_PHI_H : S_SOF1;
                    break;
                case S_PHI_H:
                    phi_h = b;
                    state = S_PHI_L;
                    break;
                case S_PHI_L:
                    phi_l = b;
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
                        int16_t phi_tenth = (int16_t)((phi_h << 8) | phi_l);
                        float angle = phi_tenth / 10.0f;
                        ESP_LOGI(TAG, "Kut: %.1f deg | Jakost: %d", angle, str_val);
                        web_server_send_data(angle, 0.0f, str_val);
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