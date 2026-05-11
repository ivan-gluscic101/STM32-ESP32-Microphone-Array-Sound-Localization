#include "uart_receiver.h"
#include "web_server.h" // Za slanje podataka na WebSocket
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h" // Za parsiranje JSON-a

static const char *TAG = "UART_RX";

#define UART_PORT UART_NUM_1
#define UART_RX_PIN GPIO_NUM_9  // GPIO za RX od STM32
#define UART_TX_PIN GPIO_NUM_10 // GPIO za TX prema STM32 (ako je potrebno, inače može biti -1)
#define UART_BUF_SIZE 256
#define UART_BAUD_RATE 921600

static void uart_rx_task(void *pvParameters) {
    // Povećaj veličinu buffera ako očekuješ veće poruke
    static uint8_t *data = (uint8_t *) malloc(UART_BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Neuspjelo alociranje memorije za UART buffer");
        vTaskDelete(NULL);
    }

    size_t rx_bytes_len = 0;
    cJSON *root = NULL;
    char json_buffer[UART_BUF_SIZE]; // Buffer za JSON string
    int json_buffer_idx = 0;

    ESP_LOGI(TAG, "UART prijemnik task pokrenut na baud %d", UART_BAUD_RATE);

    while (1) {
        rx_bytes_len = uart_read_bytes(UART_PORT, data + json_buffer_idx, UART_BUF_SIZE - json_buffer_idx - 1, 20 / portTICK_PERIOD_MS);

        if (rx_bytes_len > 0) {
            json_buffer_idx += rx_bytes_len;
            data[json_buffer_idx] = '\0'; // Null-terminiraj primljeni podatak

            // Pretražujemo delimiter (newline)
            char *newline_pos = strchr((char *)data, '\n');
            if (newline_pos != NULL) {
                *newline_pos = '\0'; // Odreži string na newlineu

                ESP_LOGI(TAG, ">>> STM32 RAW: %s", (char *)data);

                root = cJSON_Parse((char *)data);
                if (root == NULL) {
                    ESP_LOGE(TAG, "Greška pri parsiranju JSON-a: %s", (char *)data);
                } else {
                    cJSON *azimuth_obj = cJSON_GetObjectItemCaseSensitive(root, "azimuth");
                    cJSON *strength_obj = cJSON_GetObjectItemCaseSensitive(root, "strength");
                    cJSON *polar_obj = cJSON_GetObjectItemCaseSensitive(root, "polar"); // Očekujemo i polar kut

                    if (cJSON_IsNumber(azimuth_obj) && cJSON_IsNumber(strength_obj)) {
                        float azimuth = (float)azimuth_obj->valuedouble;
                        float polar = 0.0f; // Defaultna vrijednost ako polar nije poslan
                        if (cJSON_IsNumber(polar_obj)) {
                            polar = (float)polar_obj->valuedouble;
                        }
                        uint8_t strength = (uint8_t)strength_obj->valueint;

                        ESP_LOGI(TAG, "Parsirani podaci: Azimuth=%.1f, Polar=%.1f, Strength=%u", azimuth, polar, strength);
                        web_server_send_data(azimuth, polar, strength);
                    } else {
                        ESP_LOGE(TAG, "JSON ne sadrži očekivane numeričke vrijednosti za 'azimuth' ili 'strength'.");
                    }
                    cJSON_Delete(root);
                }

                // Pomakni preostale podatke (ako ih ima) na početak buffera
                size_t remaining_len = strlen(newline_pos + 1);
                if (remaining_len > 0) {
                    memmove(data, newline_pos + 1, remaining_len);
                }
                json_buffer_idx = remaining_len;
            } else if (json_buffer_idx >= UART_BUF_SIZE - 1) {
                ESP_LOGW(TAG, "Buffer pun bez newlinea, resetiram...");
                json_buffer_idx = 0;
            }
        }
    }
}

void uart_receiver_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "UART Receiver inicijaliziran.");
}