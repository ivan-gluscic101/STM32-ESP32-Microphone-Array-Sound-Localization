#include "uart_receiver.h"
#include "web_server.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "UART_RX";

#define UART_PORT UART_NUM_1
#define UART_RX_PIN GPIO_NUM_9
#define UART_TX_PIN GPIO_NUM_10
#define UART_BUF_SIZE 256
#define UART_BAUD_RATE 921600

#define FRAME_SOF1 0xAA
#define FRAME_SOF2 0xBB
#define FRAME_EOF1 0xCC
#define FRAME_EOF2 0xDD
#define PACKET_TYPE_3D 0x03

typedef enum {
    STATE_IDLE = 0,
    STATE_SOF1,
    STATE_SOF2,
    STATE_TYPE,
    STATE_AZ_H,
    STATE_AZ_L,
    STATE_EL_H,
    STATE_EL_L,
    STATE_STR,
    STATE_EOF1,
    STATE_EOF2
} parser_state_t;

static void uart_rx_task(void *pvParameters) {
    static uint8_t data[UART_BUF_SIZE];
    parser_state_t state = STATE_IDLE;
    uint8_t rx_byte;
    size_t rx_bytes_len;

    int16_t az_tenth = 0;
    int16_t el_tenth = 0;
    uint8_t strength = 0;
    uint8_t packet_type = 0;

    ESP_LOGI(TAG, "UART RX task pokrenut na %d baud", UART_BAUD_RATE);

    while (1) {
        rx_bytes_len = uart_read_bytes(UART_PORT, data, 1, 20 / portTICK_PERIOD_MS);

        if (rx_bytes_len > 0) {
            rx_byte = data[0];

            switch (state) {
                case STATE_IDLE:
                    if (rx_byte == FRAME_SOF1) {
                        state = STATE_SOF1;
                    }
                    break;

                case STATE_SOF1:
                    if (rx_byte == FRAME_SOF2) {
                        state = STATE_SOF2;
                    } else {
                        state = STATE_IDLE;
                    }
                    break;

                case STATE_SOF2:
                    packet_type = rx_byte;
                    if (packet_type == PACKET_TYPE_3D) {
                        state = STATE_TYPE;
                    } else {
                        state = STATE_IDLE;
                    }
                    break;

                case STATE_TYPE:
                    az_tenth = (int16_t)(((uint16_t)rx_byte) << 8);
                    state = STATE_AZ_H;
                    break;

                case STATE_AZ_H:
                    az_tenth |= (uint16_t)rx_byte;
                    state = STATE_AZ_L;
                    break;

                case STATE_AZ_L:
                    el_tenth = (int16_t)(((uint16_t)rx_byte) << 8);
                    state = STATE_EL_H;
                    break;

                case STATE_EL_H:
                    el_tenth |= (uint16_t)rx_byte;
                    state = STATE_EL_L;
                    break;

                case STATE_EL_L:
                    strength = rx_byte;
                    state = STATE_STR;
                    break;

                case STATE_STR:
                    if (rx_byte == FRAME_EOF1) {
                        state = STATE_EOF1;
                    } else {
                        state = STATE_IDLE;
                    }
                    break;

                case STATE_EOF1:
                    if (rx_byte == FRAME_EOF2) {
                        float azimuth = az_tenth / 10.0f;
                        float polar = el_tenth / 10.0f;
                        ESP_LOGI(TAG, ">>> Paket: Az=%.1f°, El=%.1f°, Str=%u", azimuth, polar, strength);
                        web_server_send_data(azimuth, polar, strength);
                    }
                    state = STATE_IDLE;
                    break;

                default:
                    state = STATE_IDLE;
                    break;
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
