/*
 * EMA_FILTER.c
 * Implementirano: 2026-04-03
 *
 * Phil's Lab - The Simplest Digital Filters #92
 *
 * EMA_Update() obrađuje cijeli half-buffer in-place:
 *   HP formula: y[n] = 1/2*(2-β)*(x[n] - x[n-1]) - (1-β)*y[n-1]
 *   rezultat zapiše natrag u buf[] kao uint16.
 * Stanje filtra (output[], input[]) prenosi se između poziva — filtar je
 * kontinuiran kroz ping i pong buffere.
 */

#include "EMA_FILTER.h"

void EMA_Init(ema_t *filt)
{
    for (int ch = 0; ch < NUM_CH; ch++)
    {
        filt->output[ch] = 0.0f;
        filt->input[ch]  = 0.0f;
    }
}

void EMA_Update(ema_t *filt, uint16_t *buf)
{
    for (int s = 0; s < HALF_SIZE; s++)
    {
        for (int ch = 0; ch < NUM_CH; ch++)
        {
            float x = (float)buf[s * NUM_CH + ch];
            filt->output[ch] = 0.5f * (2.0f - BETA) * (x - filt->input[ch]) - (1.0f - BETA) * filt->output[ch];
            filt->input[ch] = x;
            buf[s * NUM_CH + ch] = (uint16_t)filt->output[ch];
        }
    }
}
