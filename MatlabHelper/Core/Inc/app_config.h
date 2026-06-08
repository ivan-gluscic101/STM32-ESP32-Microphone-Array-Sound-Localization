#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Event-triggered TDOA akvizicija za FTDI FT232RL (max 3 Mbaud VCP).
 * -------------------------------------------------------------------------
 * Ranija izvedba slala je svaki uzorak kontinuirano (4 kan. × 32 kHz × 16 bit
 * ~ 2.56 Mbit/s payloada). To je trajno trošilo ~85% 3 Mbaud linije, nije
 * ostavljalo zraka hostu, a PC nije stizao trošiti okvire dovoljno brzo
 * -> preljev serijskog buffera -> gubitak sinkronizacije -> većina pljeskova
 * se gubila.
 *
 * Pljesak je tranzijent (<5 ms) koji se događa rijetko. Zato više ne šaljemo
 * sve. ADC neprekidno radi u circular DMA-u (služi kao pre-trigger ring).
 * STM32 sam detektira pljesak (energija po bloku vs prilagodljivi šumni pod)
 * i šalje SAMO kratak prozor oko događaja: EVENT_PRE_BLOCKS prije okidanja +
 * blok okidanja + EVENT_POST_BLOCKS poslije. Jedan mali paket po pljesku,
 * ostatak vremena linija miruje, pa host nikad ne preljeva i detekcija je
 * pouzdana.
 * ------------------------------------------------------------------------- */
#define ADC_NUM_CHANNELS       4U
#define ADC_SAMPLE_RATE_HZ     32000U
#define UART_BAUD_RATE         3000000U

#define FRAME_SYNC_U16         0xFFFFU
#define FRAME_SYNC_LEN         2U                 /* 2 words = 4 sync bytes */

/* TIM3 clock = APB1 timer clock = 170 MHz uz zadano stablo taktova.
 * ARR = floor(170e6 / 32000) - 1 = 5311 -> efektivni FS = 32003.012 Hz.
 * MATLAB mora koristiti isti efektivni FS. */
#define TIM3_TIMER_CLOCK_HZ    170000000U
#define TIM3_AUTORELOAD        ((TIM3_TIMER_CLOCK_HZ / ADC_SAMPLE_RATE_HZ) - 1U)
#define ADC_EFFECTIVE_FS_HZ    ((float)TIM3_TIMER_CLOCK_HZ / (float)(TIM3_AUTORELOAD + 1U))

/* -------------------------------------------------------------------------
 * Blok-bazirana akvizicija (= granularnost energetske detekcije)
 * -------------------------------------------------------------------------
 * ADC DMA je double buffer od 2 bloka; HT/TC okidaju jednom po završenom
 * bloku, tj. svakih ADC_BLOCK_SAMPLES / FS sekundi. Svaki gotov blok kopira
 * se u povijesni prsten i procjenjuje mu se energija.
 *   ADC_BLOCK_SAMPLES = 64 -> 64 / 32003 = 2.0 ms po bloku (500 Hz IRQ). */
#define ADC_BLOCK_SAMPLES      64U
#define ADC_BLOCK_LEN          (ADC_NUM_CHANNELS * ADC_BLOCK_SAMPLES)   /* halfwordova po bloku */

/* -------------------------------------------------------------------------
 * Prozor događaja koji se šalje na detektirani pljesak
 * -------------------------------------------------------------------------
 *   PRE  = 4 bloka   =  8 ms prije okidanja (pre-roll iz prstena)
 *   POST = 12 blokova = 24 ms poslije okidanja
 *   WIN  = PRE + okidanje + POST = 17 blokova = 1088 uzoraka/kan ~ 34 ms */
#define EVENT_PRE_BLOCKS       4U
#define EVENT_POST_BLOCKS      12U
#define EVENT_WIN_BLOCKS       (EVENT_PRE_BLOCKS + 1U + EVENT_POST_BLOCKS)
#define EVENT_WIN_SAMPLES      (EVENT_WIN_BLOCKS * ADC_BLOCK_SAMPLES)   /* po kanalu */

/* Povijesni prsten (pre-trigger buffer). Potencija dvojke pa se indeks omata
 * maskom. Mora udobno premašiti EVENT_WIN_BLOCKS da se najstariji blok prozora
 * ne prepiše prije nego ga glavna petlja iskopira.
 *   32 bloka = 64 ms povijesti (prozor je 17 blokova). */
#define HIST_BLOCKS            32U
#define HIST_MASK              (HIST_BLOCKS - 1U)

/* TX paket = sync zaglavlje + interleaveani uzorci prozora. */
#define ADC_FRAME_LEN          (EVENT_WIN_BLOCKS * ADC_BLOCK_LEN)       /* payload halfwords */
#define ADC_TX_LEN             (FRAME_SYNC_LEN + ADC_FRAME_LEN)
#define ADC_TX_BYTES           (ADC_TX_LEN * 2U)                        /* = 8708 B / pljesak */

/* -------------------------------------------------------------------------
 * Parametri detekcije (ako STM32 pre- ili pod-okida, prvo podesi
 * EVENT_TRIG_FACTOR / EVENT_ABS_MIN_ENERGY).
 * -------------------------------------------------------------------------
 * Okini kad energija bloka premaši šumni_pod * EVENT_TRIG_FACTOR I apsolutni
 * pod (da potpuna tišina s NF ~ 0 ne može sama sebe okinuti).
 *   faktor 8 ~ +9 dB, usporedivo sa starim MATLAB pragom od +10 dB. */
#define EVENT_TRIG_FACTOR        8U      /* relativni okid: e > NF * faktor      */
#define EVENT_ABS_MIN_ENERGY     20000U  /* apsolutni energetski pod za blok     */
#define EVENT_NF_SHIFT           6U      /* IIR šumnog poda: nf += (e-nf)>>shift */
#define EVENT_DC_SHIFT           9U      /* vrem. konstanta DC pratitelja/kanalu */
#define EVENT_REFRACTORY_BLOCKS  125U    /* ignoriraj re-okide ~250 ms nakon ev. */
#define EVENT_NF_SEED_BLOCKS     32U     /* nauči šumni pod prije naoružavanja   */

#endif /* APP_CONFIG_H */
