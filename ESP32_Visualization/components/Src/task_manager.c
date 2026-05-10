#include "task_manager.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

// Ovdje dodaj headere gdje su definirane tvoje funkcije taskova
// #include "acquisition.h"
// #include "processing.h"
// #include "communication.h"

/* Eksterne deklaracije ako funkcije nisu u headerima */
extern void ACQ_Task(void *argument);
extern void FFT_Task(void *argument);
extern void UART_Task(void *argument);

/**
 * Privremeni testni task koji simulira rotirajući izvor zvuka
 */
void Mock_Test_Task(void *argument) {
    char msg[64];
    float mock_azimuth = 0.0f;
    uint32_t start_time = xTaskGetTickCount();

    while (1) {
        mock_azimuth += 5.0f;
        if (mock_azimuth >= 360.0f) mock_azimuth = 0.0f;

        // Formatiramo JSON string koji ESP32 i Web sučelje očekuju
        int len = snprintf(msg, sizeof(msg), "{\"azimuth\": %.1f, \"strength\": 180}\n", mock_azimuth);
        
        // Slanje direktno na UART1 (podesi huart1 ako je drugi port u pitanju)
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, len, 100);

        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz osvježavanje
    }
}

void app_tasks_init(void) {
    /* Kreiranje taskova - prioritet je ključan */
    xTaskCreate(ACQ_Task,  "ACQ",  1024, NULL, 5, NULL);
    xTaskCreate(FFT_Task,  "FFT",  2048, NULL, 4, NULL);
    xTaskCreate(UART_Task, "UART", 512,  NULL, 3, NULL);

    /* Pokretanje testnog taska */
    xTaskCreate(Mock_Test_Task, "MOCK", 512, NULL, 2, NULL);
}