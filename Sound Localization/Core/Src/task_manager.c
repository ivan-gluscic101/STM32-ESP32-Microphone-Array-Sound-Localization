#include "task_manager.h"
#include "uart_driver.h"
#include <stdio.h>
#include <string.h>

/* Eksterne deklaracije ako funkcije nisu u headerima */
extern void ACQ_Task(void *argument) __attribute__((weak));
extern void FFT_Task(void *argument) __attribute__((weak));
extern void UART_Task(void *argument) __attribute__((weak));

/**
 * Testni task koji simulira rotirajući zvučni izvor sa 3D kutovima
 * Šalje binarni format: [0xAA][0xBB][0x03][AZ_H][AZ_L][EL_H][EL_L][STR][0xCC][0xDD]
 */
void Mock_Test_Task(void *argument) {
    float mock_azimuth = 0.0f;
    float mock_elevation = 30.0f;  /* Inicijalna elevacija */
    uint8_t mock_strength = 180;
    uint32_t start_time = xTaskGetTickCount();

    /* Testira se 60 sekundi */
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(60000)) {
        mock_azimuth += 10.0f;
        if (mock_azimuth >= 360.0f) mock_azimuth = 0.0f;

        /* Elevacija oscilira između -45 i +45 stupnjeva */
        mock_elevation = 45.0f * sinf(mock_azimuth * 3.14159f / 180.0f);

        /* Konverzija u desetine stupnja (za protokol) */
        int16_t az_tenth = (int16_t)(mock_azimuth * 10.0f);
        int16_t el_tenth = (int16_t)(mock_elevation * 10.0f);

        /* Slanje binarnog 3D paketa na UART4 */
        UART_SendAngle3DPacket(az_tenth, el_tenth, mock_strength);

        vTaskDelay(pdMS_TO_TICKS(200));  /* 5 Hz osvježavanje */
    }

    vTaskDelete(NULL);
}

void app_tasks_init(void) {
    /* Pokretanje testnog taska s visokim prioritetom da budemo sigurni da radi */
    xTaskCreate(Mock_Test_Task, "MOCK_TEST", 512, NULL, 6, NULL);
}
