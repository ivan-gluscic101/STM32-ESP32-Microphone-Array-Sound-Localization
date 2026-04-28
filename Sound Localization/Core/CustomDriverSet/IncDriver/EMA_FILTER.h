/* Eksponencijalni pomični prosjek — visokofrekventni filtar (high-pass).
 * Phil's Lab - The Simplest Digital Filters #92
 * EMA_FILTER.h
 * Implementirano: 2026-04-03
 *
 * HP formula: y[n] = 1/2*(2-BETA)*(x[n] - x[n-1]) - (1-BETA)*y[n-1]
 * BETA = 0 → nema filtriranja (prolaz), BETA = 1 → maksimalno filtriranje
 */

#ifndef CUSTOMDRIVERSET_INCDRIVER_EMA_FILTER_H_
#define CUSTOMDRIVERSET_INCDRIVER_EMA_FILTER_H_

#include <stdint.h>

#define BETA      0.7f
#define NUM_CH    4
#define HALF_SIZE 256

typedef struct {
    float output[NUM_CH];   /* prethodni izlaz filtra po kanalu — stanje filtra */
    float input[NUM_CH];  /* prethodni ulaz filtra po kanalu */
} ema_t;

void EMA_Init(ema_t *filt);
void EMA_Update(ema_t *filt, uint16_t *buf);

#endif /* CUSTOMDRIVERSET_INCDRIVER_EMA_FILTER_H_ */
