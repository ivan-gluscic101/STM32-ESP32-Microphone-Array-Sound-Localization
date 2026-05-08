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

/**
 * Task koji čita UART i traži "Angle Packet" od STM32.
 */
static void rx_task(void *arg) {
    uint8_t *data = (uint8_t *) malloc(UART_BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Neuspjela alokacija buffera!");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            for (int i = 0; i < len - (ANGLE_PKT_LEN - 1); i++) {
                // Provjera markera (0xAA, 0xBB) i tipa paketa (0x02)
                if (data[i] == ANGLE_PKT_SOF1 && data[i+1] == ANGLE_PKT_SOF2 && data[i+2] == ANGLE_PKT_TYPE) {
                    
                    // Provjera EOF markera (0xCC, 0xDD)
                    if (data[i+6] == ANGLE_PKT_EOF1 && data[i+7] == ANGLE_PKT_EOF2) {
                        
                        int16_t phi_tenth = (int16_t)((data[i+3] << 8) | data[i+4]);
                        uint8_t strength = data[i+5];
                        float angle = phi_tenth / 10.0f;
                        
                        ESP_LOGI(TAG, "Kut: %.1f deg | Jakost: %d", angle, strength);
                        
                        // Slanje podataka na Web vizualizaciju
                        web_server_send_data(angle, strength);
                        
                        i += (ANGLE_PKT_LEN - 1); 
                    }
                }
            }
        }
    }
    free(data);
}

void sound_loc_processor_init(void) {
    ESP_LOGI(TAG, "Inicijalizacija procesora kuta...");
    xTaskCreate(rx_task, "uart_rx_task", 4096, NULL, 10, NULL);
}