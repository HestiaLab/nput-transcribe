#include "nput/audio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    /* 插值中心两侧的抽头数。对当前处理的采样率，16 个足以覆盖语音频带，
       同时保持线性且较小的计算量。 */
    NPUT_RESAMPLE_HALF_TAPS = 16
};

static float *copy_samples(const float *source, size_t count) {
    float *copy = (float *)malloc((count == 0 ? 1 : count) * sizeof(float));
    if (copy == NULL) {
        return NULL;
    }
    if (count > 0) {
        memcpy(copy, source, count * sizeof(float));
    }
    return copy;
}

nput_status nput_audio_resample(const nput_audio *source, int target_rate, nput_audio *out) {
    if (source == NULL || out == NULL || target_rate <= 0) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    nput_audio_init(out);
    if (source->sample_rate <= 0) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    if (source->count == 0) {
        out->sample_rate = target_rate;
        out->samples = (float *)malloc(sizeof(float));
        return out->samples == NULL ? NPUT_ERR_OUT_OF_MEMORY : NPUT_OK;
    }
    if (source->sample_rate == target_rate) {
        float *copy = copy_samples(source->samples, source->count);
        if (copy == NULL) {
            return NPUT_ERR_OUT_OF_MEMORY;
        }
        out->samples = copy;
        out->count = source->count;
        out->sample_rate = target_rate;
        return NPUT_OK;
    }

    const double ratio = (double)target_rate / (double)source->sample_rate;
    const size_t out_count = (size_t)((double)source->count * ratio + 0.5);
    float *dst = (float *)malloc((out_count == 0 ? 1 : out_count) * sizeof(float));
    if (dst == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }

    /* 低通截止取两个奈奎斯特频率中较低者，避免降采样混叠，并保留 95% 的通带。 */
    const double cutoff = (ratio < 1.0 ? ratio : 1.0) * 0.95;
    const double half = (double)NPUT_RESAMPLE_HALF_TAPS;

    for (size_t i = 0; i < out_count; ++i) {
        const double centre = (double)i / ratio;
        const long long first = (long long)floor(centre) - NPUT_RESAMPLE_HALF_TAPS + 1;
        double accumulator = 0.0;
        double weight_sum = 0.0;
        for (int tap = 0; tap < 2 * NPUT_RESAMPLE_HALF_TAPS; ++tap) {
            const long long index = first + tap;
            if (index < 0 || (size_t)index >= source->count) {
                continue;
            }
            const double distance = centre - (double)index;
            const double x = M_PI * cutoff * distance;
            const double sinc = fabs(x) < 1e-12 ? 1.0 : sin(x) / x;
            const double t = distance / half;
            const double window = 0.42 + 0.5 * cos(M_PI * t) + 0.08 * cos(2.0 * M_PI * t);
            const double weight = cutoff * sinc * window;
            accumulator += (double)source->samples[index] * weight;
            weight_sum += weight;
        }
        dst[i] = weight_sum != 0.0 ? (float)(accumulator / weight_sum) : 0.0f;
    }

    out->samples = dst;
    out->count = out_count;
    out->sample_rate = target_rate;
    return NPUT_OK;
}
