#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * TDOA stream configuration for FTDI FT232RL
 * -------------------------------------------------------------------------
 * FT232RL is limited to 3 Mbaud in UART/VCP mode. With UART 8N1 each byte
 * uses 10 line bits. For 4 channels at 16 bit:
 *   line_rate = 4 * sample_rate * 2 * 10
 * At 64 kHz this is about 5.12 Mbit/s, so it cannot be streamed through
 * FT232RL. 32 kHz uses about 2.56 Mbit/s payload line rate and leaves
 * practical margin below 3 Mbaud.
 */
#define ADC_NUM_CHANNELS       4U
#define ADC_SAMPLES_PER_CH     1024U
#define ADC_SAMPLE_RATE_HZ     32000U
#define UART_BAUD_RATE         3000000U

#define FRAME_SYNC_U16         0xFFFFU
#define FRAME_SYNC_LEN         2U                 /* 2 words = 4 sync bytes */
#define ADC_FRAME_LEN          (ADC_NUM_CHANNELS * ADC_SAMPLES_PER_CH)
#define ADC_TX_LEN             (FRAME_SYNC_LEN + ADC_FRAME_LEN)
#define ADC_TX_BYTES           (ADC_TX_LEN * 2U)

/* TIM3 clock is APB1 timer clock. With the provided clock tree it is 170 MHz.
 * Integer division gives ARR = floor(170e6 / 32000) - 1 = 5311.
 * Effective FS = 170e6 / (5311 + 1) = 32003.012 Hz. MATLAB should
 * use the same effective FS below.
 */
#define TIM3_TIMER_CLOCK_HZ    170000000U
#define TIM3_AUTORELOAD        ((TIM3_TIMER_CLOCK_HZ / ADC_SAMPLE_RATE_HZ) - 1U)
#define ADC_EFFECTIVE_FS_HZ    ((float)TIM3_TIMER_CLOCK_HZ / (float)(TIM3_AUTORELOAD + 1U))

#endif /* APP_CONFIG_H */
