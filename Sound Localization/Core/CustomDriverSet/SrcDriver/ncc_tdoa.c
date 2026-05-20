#include "ncc_tdoa.h"
#include <math.h>

void NCC_ExtractChannels(const uint16_t *buf,
                         float ch0[], float ch1[],
                         float ch2[], float ch3[])
{
    float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f;
    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        sum0 += (float)buf[s * NUM_CH + 0];
        sum1 += (float)buf[s * NUM_CH + 1];
        sum2 += (float)buf[s * NUM_CH + 2];
        sum3 += (float)buf[s * NUM_CH + 3];
    }
    float inv_n = 1.0f / SAMPLES_PER_CHANNEL;
    float dc0 = sum0 * inv_n;
    float dc1 = sum1 * inv_n;
    float dc2 = sum2 * inv_n;
    float dc3 = sum3 * inv_n;

    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        ch0[s] = (float)buf[s * NUM_CH + 0] - dc0;
        ch1[s] = (float)buf[s * NUM_CH + 1] - dc1;
        ch2[s] = (float)buf[s * NUM_CH + 2] - dc2;
        ch3[s] = (float)buf[s * NUM_CH + 3] - dc3;
    }
}

float NCC_RMS(const float *ch)
{
    float acc = 0.0f;
    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        acc += ch[s] * ch[s];
    }
    return sqrtf(acc / (float)SAMPLES_PER_CHANNEL);
}

int32_t NCC_ComputeLag(const float *ref, const float *sig)
{
    float e_ref = 0.0f, e_sig = 0.0f;
    for (int s = 0; s < SAMPLES_PER_CHANNEL; s++) {
        e_ref += ref[s] * ref[s];
        e_sig += sig[s] * sig[s];
    }
    float norm = sqrtf(e_ref * e_sig);
    if (norm < 1e-6f) return 0;

    float   best_val = -2.0f;
    int32_t best_lag = 0;

    for (int32_t lag = -TDOA_MAX_SAMPLES; lag <= TDOA_MAX_SAMPLES; lag++) {
        int s_start = (lag < 0) ? -lag : 0;
        int s_end   = (lag > 0) ? SAMPLES_PER_CHANNEL - lag : SAMPLES_PER_CHANNEL;

        float acc = 0.0f;
        for (int s = s_start; s < s_end; s++) {
            acc += ref[s] * sig[s + lag];
        }
        acc /= norm;

        if (acc > best_val) {
            best_val = acc;
            best_lag = lag;
        }
    }
    return best_lag;
}
