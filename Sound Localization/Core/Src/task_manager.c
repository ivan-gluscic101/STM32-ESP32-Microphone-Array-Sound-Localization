#include "task_manager.h"
#include "uart_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <string.h>

// Ovdje dodaj headere gdje su definirane tvoje funkcije taskova
// #include "acquisition.h"
// #include "processing.h"
// #include "communication.h"

/* Eksterne deklaracije ako funkcije nisu u headerima */
extern void StartACQTask(void *argument);
extern void StartFFTTask(void *argument);
extern void StartUARTTask(void *argument);

/**
 * Privremeni testni task koji simulira rotirajući izvor zvuka sa 3D lokalizacijom
 */
void Mock_Test_Task(void *argument) {
    int16_t az_tenth = 0;
    int16_t el_tenth = 0;
    uint8_t strength = 100;

    while (1) {
        az_tenth += 50;                     // +5° po koraku
        if (az_tenth >= 3600) az_tenth = 0;

        // Simulacija promjene elevacije (0-180 stupnjeva u desetinkama)
        el_tenth = (el_tenth + 10) % 1800;

        // Slanje 3D paketa (binarni format: 10 bajtova)
        UART_SendAngle3DPacket(az_tenth, el_tenth, strength);

        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz osvježavanje
    }
}

void app_tasks_init(void) {
    /* Pokretanje testnog mock taska koji simulira 3D lokalizaciju */
    xTaskCreate(Mock_Test_Task, "MOCK", 512, NULL, 2, NULL);
}
