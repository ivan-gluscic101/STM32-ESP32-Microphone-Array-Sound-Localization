#include "task_manager.h"
#include "adc_driver.h"
#include "uart_driver.h"
#include "audio_common.h"
#include "raw_capture.h"
#include "mpu9250.h"
#include "cmsis_os.h"
#include <string.h>

/* Lokalizacijski mod biran u audio_common.h. Aliasi ispod čine ostatak koda
 * neovisnim o izabranoj verziji: isti tip rezultata i isti pozivi.
 * Prioritet: USE_4MIC_TIME_LOC > 3-mic time-domain (default).
 * FFT/GCC-PHAT verzije (sound_loc_3d, loc3d_3mic) su uklonjene — sve su
 * time-domain (TDOA pragom). USE_TIME_DOMAIN_LOC se zadržava radi kompatibilnosti
 * konfiguracije, ali je jedina podržana vrijednost 1. */
#if USE_4MIC_TIME_LOC
  #include "sound_loc3d_4mic_time.h"
  typedef loc3d_4mic_time_result_t loc_result_t;
  #define LOC_INIT()            LOC3D_4MIC_TIME_Init()
  #define LOC_PROCESS(b, r)     LOC3D_4MIC_TIME_Process((b), (r))
#else
  #if defined(USE_TIME_DOMAIN_LOC) && !USE_TIME_DOMAIN_LOC
    #error "FFT/GCC-PHAT lokalizacija je uklonjena. Postavi USE_TIME_DOMAIN_LOC=1 ili USE_4MIC_TIME_LOC=1."
  #endif
  #include "loc3d_3mic_time.h"
  typedef loc3d_3mic_time_result_t loc_result_t;
  #define LOC_INIT()            LOC3D_3MIC_TIME_Init()
  #define LOC_PROCESS(b, r)     LOC3D_3MIC_TIME_Process((b), (r))
#endif

/* USE_MOCK_ADC je definiran u audio_common.h (dijeli ga mock_adc.c radi
 * uvjetnog kompajliranja mock tablica). */
#define ACQ_NUM_BUFFERS  3

osMessageQId  queueDmaEventHandle;
QueueHandle_t queueSnapshotHandle;
QueueHandle_t queueResultHandle;

/* Štiti UART4 — i UART_Task (lokalizacija 0x03) i IMU_Task (orijentacija 0x05)
 * šalju paralelno. Bez mutexa bi se bajtovi dvaju paketa izmiješali na žici. */
static osMutexId uartMutexHandle;

/* Trostruki snapshot buffer + FIFO queue indeksa (dubina ACQ_NUM_BUFFERS-1):
 * ACQ_Task rotira 0→1→2→0 i šalje indeks, LOC_Task ih troši redom (FIFO).
 * Queue dubine N-1 + 1 koji se trenutno čita = max N "zauzetih" buffera, pa
 * ACQ nikad ne prepisuje buffer koji je u queue-u ili se čita (vidi guard dolje).
 * 3 × HALF_BUFFER × 2 B = 3 × 8 KB = 24 KB u BSS-u. */
static uint16_t acq_snapshot[ACQ_NUM_BUFFERS][HALF_BUFFER];

/* Sliding window: drži [prethodni half | trenutni half] = 2 × HALF_BUFFER uzoraka.
 * Vlasništvo: isključivo LOC_Task — nema concurrency problema.
 * 2 × HALF_BUFFER × 2 B = 16 KB u BSS-u. */
static uint16_t sliding_buf[2 * HALF_BUFFER];

/* Raw CSV se šalje IZRAVNO iz dbg_raw_chX (raw_capture.c). capture_ready zamrzava
 * obradu između detekcije i kraja UART slanja (CSV ~2 s @115200): dok je
 * postavljen, LOC_Task ne zove LOC_PROCESS pa RawCapture_Snapshot ne prepisuje
 * dbg_raw_chX koji UART_Task upravo ispisuje. Producer: LOC_Task. Consumer: UART_Task. */
static volatile uint8_t capture_ready = 0;

/* ── Task implementacije ─────────────────────────────────────────────────── */

static void StartACQTask(void const *argument)
{
    uint8_t write_idx = 0;

    for (;;) {
        osEvent evt = osMessageGet(queueDmaEventHandle, osWaitForever);
        if (evt.status != osEventMessage) { continue; }

        /* Ako LOC_Task zaostaje i queue je pun, preskoči ovaj event umjesto da
         * prepišemo acq_snapshot[write_idx] koji je možda još u queue-u ili ga
         * LOC_Task upravo čita — time bismo izazvali torn read. Samo proizvođač
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

static void StartLOCTask(void const *argument)
{
    loc_result_t result;
    uint8_t read_idx;

    for (;;) {
        if (xQueueReceive(queueSnapshotHandle, &read_idx, portMAX_DELAY) != pdTRUE) { continue; }

        memcpy(&sliding_buf[0],
               &sliding_buf[HALF_BUFFER],
               HALF_BUFFER * sizeof(uint16_t));
        memcpy(&sliding_buf[HALF_BUFFER],
               acq_snapshot[read_idx],
               HALF_BUFFER * sizeof(uint16_t));

        /* Dok prethodni raw CSV još nije ispisan, ne pokreći obradu — inače bi
         * RawCapture_Snapshot prepisao dbg_raw_chX koji UART_Task upravo šalje.
         * Sliding buffer se i dalje održava gore, pa ne gubimo kontinuitet. */
        if (!capture_ready) {
            /* LOC_PROCESS interno traži energetski vrh, centrira prozor te
             * napuni dbg_raw_chX (RawCapture_Snapshot) pri detekciji. */
            if (LOC_PROCESS(sliding_buf, &result)) {
                capture_ready = 1;   /* dbg_raw_chX sada drži ovaj prozor */
                xQueueSend(queueResultHandle, &result, 0);
            }
        }
    }
}

static void StartUARTTask(void const *argument)
{
    loc_result_t r;

    for (;;) {
        if (xQueueReceive(queueResultHandle, &r, portMAX_DELAY) == pdTRUE) {
            osMutexWait(uartMutexHandle, osWaitForever);
            UART_SendAngle3DPacket(r.az_tenth, r.el_tenth, r.strength);

            /* Raw CSV ispis uzoraka prozora detekcije zasad je ISKLJUČEN. Sadržaj
             * prozora i dalje stoji u dbg_raw_chX (dostupno u debuggeru). Za
             * ponovni ispis na ESP32 monitor odkomentiraj: */
            /* if (capture_ready) {
                UART_SendRawCaptureCSV(dbg_raw_ch0, dbg_raw_ch1,
                                       dbg_raw_ch2, dbg_raw_ch3);
            } */
            osMutexRelease(uartMutexHandle);

            /* Otpusti zamrzavanje — LOC_Task smije ponovno tražiti detekcije. */
            capture_ready = 0;
        }
    }
}

/*
 * IMU_Task — čita MPU-9250, fuzira orijentaciju (Mahony), šalje paket 0x05.
 *
 * Prioritet BelowNormal: ispod ACQ_Task (Realtime) i LOC_Task (High) — nikad ne
 * ometa akviziciju ni lokalizaciju. Iznad UART_Task (Low) nije nužno; oba dijele
 * UART preko uartMutexHandle.
 *
 * Petlja radi na ~100 Hz (osDelay 10 ms) radi glatke fuzije, ali UART paket
 * šalje rjeđe (svakih ~5 iteracija ≈ 20 Hz) — orijentacija se ne mijenja brzo,
 * a time ostavljamo UART propusnost lokalizaciji.
 */
static void StartIMUTask(void const *argument)
{
    mpu_orientation_t ori;
    const float dt = 0.01f;       /* 100 Hz fuzija */
    uint8_t send_div = 0;
    uint8_t flags;

    /* Ako MPU nije detektiran u main.c, MPU_Update vraća grešku — task tada
     * samo mirno spava i ne šalje ništa (ploča radi bez IMU-a). */
    flags = MPU_HasMag() ? 0x01u : 0x00u;

    for (;;) {
        if (MPU_Update(dt, &ori) == 0) {
            if (++send_div >= 5u) {     /* ~20 Hz slanje */
                send_div = 0;

                int16_t roll_t  = (int16_t)(ori.roll_deg  * 10.0f);
                int16_t pitch_t = (int16_t)(ori.pitch_deg * 10.0f);

                /* Bez magnetometra (npr. MPU-6500) yaw nema apsolutnu referencu
                 * i driftao bi. Šaljemo yaw=0 i flag=0 → ESP32 ga ignorira i
                 * zakreće plohu samo po pitch/roll (stabilno iz gravitacije). */
                int16_t yaw_t = flags ? (int16_t)(ori.yaw_deg * 10.0f) : 0;

                osMutexWait(uartMutexHandle, osWaitForever);
                UART_SendOrientationPacket(roll_t, pitch_t, yaw_t, flags);
                osMutexRelease(uartMutexHandle);
            }
        }
        osDelay(10);
    }
}

/* ── Inicijalizacija ─────────────────────────────────────────────────────── */

void app_tasks_init(void)
{
    LOC_INIT();   /* izračunaj M_geom iz pozicija mikrofona prije pokretanja taskova */

    osMessageQDef(queueDmaEvent, 8, uint32_t);
    queueDmaEventHandle = osMessageCreate(osMessageQ(queueDmaEvent), NULL);

    queueSnapshotHandle = xQueueCreate(ACQ_NUM_BUFFERS - 1, sizeof(uint8_t));
    queueResultHandle   = xQueueCreate(4, sizeof(loc_result_t));

    osMutexDef(uartMutex);
    uartMutexHandle = osMutexCreate(osMutex(uartMutex));

    osThreadDef(ACQ_Task,  StartACQTask,  osPriorityRealtime,    0, 256);
    (void)osThreadCreate(osThread(ACQ_Task),  NULL);

    osThreadDef(LOC_Task,  StartLOCTask,  osPriorityHigh,        0, 1024);
    (void)osThreadCreate(osThread(LOC_Task),  NULL);

    osThreadDef(UART_Task, StartUARTTask, osPriorityLow,         0, 256);
    (void)osThreadCreate(osThread(UART_Task), NULL);

    /* IMU_Task — float fuzija (Mahony) traži FPU kontekst → 512 words stacka. */
    osThreadDef(IMU_Task,  StartIMUTask,  osPriorityBelowNormal, 0, 512);
    (void)osThreadCreate(osThread(IMU_Task),  NULL);
}
