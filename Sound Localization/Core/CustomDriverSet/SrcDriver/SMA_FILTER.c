/*
 * SMA_FILTER.c — Simple Moving Average, niskofrekvencijski filtar.
 * Implementirano: 2026-04-03
 *
 * Running-sum pristup: za svaki uzorak oduzmi najstariju vrijednost iz sume,
 * upiši novu, dodaj je u sumu, podijeli s W (bit-shift).
 * Stanje (buf[], idx[], sum[]) prenosi se između ping i pong poziva.
 */

#include "SMA_FILTER.h"

void SMA_Init(sma_t *filt)
{
    for (int ch = 0; ch < NUM_CH; ch++)
    {
        filt->idx[ch] = 0;
        filt->sum[ch] = 0;
        for (int i = 0; i < SMA_WINDOW; i++)
            filt->buf[ch][i] = 0;
    }
}

void SMA_Update(sma_t *filt, uint16_t *buf)
{
    for (int s = 0; s < HALF_SIZE; s++)
    {
        for (int ch = 0; ch < NUM_CH; ch++)
        {
            uint16_t x   = buf[s * NUM_CH + ch];
            uint8_t  i   = filt->idx[ch];

            /* oduzmi najstariji, dodaj novi uzorak u tekuću sumu */
            filt->sum[ch] -= filt->buf[ch][i];
            filt->buf[ch][i] = x;
            filt->sum[ch] += x;

            /* pomakni indeks kružnog međuspremnika */
            filt->idx[ch] = (i + 1 < SMA_WINDOW) ? i + 1 : 0;

            /* bit-shift umjesto dijeljenja: >> SMA_WINDOW_SHIFT */
            buf[s * NUM_CH + ch] = (uint16_t)(filt->sum[ch] >> SMA_WINDOW_SHIFT);
        }
    }
}
