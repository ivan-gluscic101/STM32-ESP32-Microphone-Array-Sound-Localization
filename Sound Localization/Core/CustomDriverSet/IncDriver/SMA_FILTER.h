/*
 * SMA_FILTER.h — Simple Moving Average, niskofrekvencijski filtar.
 * Implementirano: 2026-04-03
 *
 * Algoritam (running-sum):
 *   y[n] = (x[n] + x[n-1] + ... + x[n-W+1]) / W
 *
 * Running-sum pristup: O(1) po uzorku — oduzmi najstariji, dodaj novi.
 * W mora biti potencija broja 2 kako bi dijeljenje postalo bit-shift (bez troška).
 *
 *   SMA_WINDOW = 4  → 4 × 62.5 µs = 0.25 ms zaglađivanje   (manje kašnjenje)
 *   SMA_WINDOW = 8  → 8 × 62.5 µs = 0.50 ms zaglađivanje   (preporučeno)
 *   SMA_WINDOW = 16 → 16 × 62.5 µs = 1.00 ms zaglađivanje  (jače filtriranje)
 */

#ifndef CUSTOMDRIVERSET_INCDRIVER_SMA_FILTER_H_
#define CUSTOMDRIVERSET_INCDRIVER_SMA_FILTER_H_

#include <stdint.h>

#define SMA_WINDOW      32   /* veličina prozora — mora biti potencija broja 2 */
#define SMA_WINDOW_SHIFT 5   /* log2(SMA_WINDOW): 2^5 = 32 */
#define NUM_CH          4
#define HALF_SIZE       256

typedef struct {
    uint16_t buf[NUM_CH][SMA_WINDOW]; /* kružni međuspremnik po kanalu        */
    uint8_t  idx[NUM_CH];             /* indeks sljedećeg upisa               */
    uint32_t sum[NUM_CH];             /* tekuća suma — izbjegava ponavljanje  */
} sma_t;

void SMA_Init(sma_t *filt);
void SMA_Update(sma_t *filt, uint16_t *buf);

#endif /* CUSTOMDRIVERSET_INCDRIVER_SMA_FILTER_H_ */
