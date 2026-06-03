#include "task_manager.h"
#include "adc_driver.h"
#include "uart_driver.h"
#include "sound_loc_3d.h"
#include "gcc_phat.h"
#include "mock_adc.h"
#include "audio_common.h"
#include <string.h>

/* USE_MOCK_ADC je definiran u audio_common.h (dijeli ga mock_adc.c radi
 * uvjetnog kompajliranja mock tablica). */
#define ACQ_NUM_BUFFERS  3

osMessageQId  queueDmaEventHandle;
QueueHandle_t queueSnapshotHandle;
QueueHandle_t queueResultHandle;

/* Trostruki snapshot buffer + FIFO queue indeksa (dubina ACQ_NUM_BUFFERS-1):
 * ACQ_Task rotira 0→1→2→0 i šalje indeks, FFT_Task ih troši redom (FIFO).
 * Queue dubine N-1 + 1 koji se trenutno čita = max N "zauzetih" buffera, pa
 * ACQ nikad ne prepisuje buffer koji je u queue-u ili se čita (vidi guard dolje).
 * 3 × HALF_BUFFER × 2 B = 3 × 8 KB = 24 KB u BSS-u. */
static uint16_t acq_snapshot[ACQ_NUM_BUFFERS][HALF_BUFFER];

/* Sliding window: drži [prethodni half | trenutni half] = 2 × HALF_BUFFER uzoraka.
 * Vlasništvo: isključivo FFT_Task — nema concurrency problema.
 * 2 × HALF_BUFFER × 2 B = 16 KB u BSS-u. */
static uint16_t sliding_buf[2 * HALF_BUFFER];

/* ── Task implementacije ─────────────────────────────────────────────────── */

static void StartACQTask(void const *argument)
{
    uint8_t write_idx = 0;

    for (;;) {
        osEvent evt = osMessageGet(queueDmaEventHandle, osWaitForever);
        if (evt.status != osEventMessage) { continue; }

        /* Ako FFT_Task zaostaje i queue je pun, preskoči ovaj event umjesto da
         * prepišemo acq_snapshot[write_idx] koji je možda još u queue-u ili ga
         * FFT_Task upravo čita — time bismo izazvali torn read. Samo proizvođač
         * šalje u ovaj queue, pa se prostor između provjere i slanja ne smanjuje. */
        if (uxQueueSpacesAvailable(queueSnapshotHandle) == 0u) { continue; }

#if USE_MOCK_ADC
        (void)evt;
        Mock_FillHalf(acq_snapshot[write_idx]);
#else
        /* flag=0: HT → prva polovica [0..HALF_BUFFER-1]
         * flag=1: TC → druga polovica [HALF_BUFFER..FULL_BUFFER-1] */
        const uint16_t *src = (evt.value.v == 0u)
                              ? &adc_buffer[0]
                              : &adc_buffer[HALF_BUFFER];
        memcpy(acq_snapshot[write_idx], src, HALF_BUFFER * sizeof(uint16_t));
#endif

        xQueueSend(queueSnapshotHandle, &write_idx, 0u);
        write_idx = (uint8_t)((write_idx + 1u) % ACQ_NUM_BUFFERS);
    }
}

static void StartFFTTask(void const *argument)
{
    loc3d_result_t result;
    uint8_t read_idx;

    for (;;) {
        if (xQueueReceive(queueSnapshotHandle, &read_idx, portMAX_DELAY) != pdTRUE) { continue; }

        memcpy(&sliding_buf[0],
               &sliding_buf[HALF_BUFFER],
               HALF_BUFFER * sizeof(uint16_t));
        memcpy(&sliding_buf[HALF_BUFFER],
               acq_snapshot[read_idx],
               HALF_BUFFER * sizeof(uint16_t));

        /* LOC3D_Process interno traži energetski vrh i centrira prozor. */
        if (LOC3D_Process(sliding_buf, &result)) {
            xQueueSend(queueResultHandle, &result, 0);
        }
    }
}

static void StartUARTTask(void const *argument)
{
    loc3d_result_t r;

    for (;;) {
        if (xQueueReceive(queueResultHandle, &r, portMAX_DELAY) == pdTRUE) {
            UART_SendAngle3DPacket(r.az_tenth, r.el_tenth, r.strength);
        }
    }
}

/* ── Inicijalizacija ─────────────────────────────────────────────────────── */

void app_tasks_init(void)
{
    LOC3D_Init();   /* izračunaj M_geom iz pozicija mikrofona prije pokretanja taskova */

    osMessageQDef(queueDmaEvent, 8, uint32_t);
    queueDmaEventHandle = osMessageCreate(osMessageQ(queueDmaEvent), NULL);

    queueSnapshotHandle = xQueueCreate(ACQ_NUM_BUFFERS - 1, sizeof(uint8_t));
    queueResultHandle   = xQueueCreate(4, sizeof(loc3d_result_t));

    osThreadDef(ACQ_Task,  StartACQTask,  osPriorityRealtime, 0, 256);
    (void)osThreadCreate(osThread(ACQ_Task),  NULL);

    osThreadDef(FFT_Task,  StartFFTTask,  osPriorityHigh,     0, 1024);
    (void)osThreadCreate(osThread(FFT_Task),  NULL);

    osThreadDef(UART_Task, StartUARTTask, osPriorityLow,      0, 256);
    (void)osThreadCreate(osThread(UART_Task), NULL);
}
