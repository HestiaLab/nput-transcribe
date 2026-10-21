#include "nput/mel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    NPUT_MEL_BINS_PER_SPECTRUM = NPUT_MEL_WINDOW / 2 + 1, /* 201 */
    NPUT_MEL_PAD = NPUT_MEL_WINDOW / 2,                    /* 200 */
    NPUT_MEL_PADDED = NPUT_MEL_SAMPLES + NPUT_MEL_WINDOW   /* 480400 */
};

/* librosa 的 Slaney mel 刻度，Whisper 的滤波器组基于它构建。 */
static double slaney_hz_to_mel(double hz) {
    const double f_sp = 200.0 / 3.0;
    const double min_log_hz = 1000.0;
    const double min_log_mel = min_log_hz / f_sp;
    const double logstep = log(6.4) / 27.0;
    if (hz >= min_log_hz) {
        return min_log_mel + log(hz / min_log_hz) / logstep;
    }
    return hz / f_sp;
}

static double slaney_mel_to_hz(double mel) {
    const double f_sp = 200.0 / 3.0;
    const double min_log_hz = 1000.0;
    const double min_log_mel = min_log_hz / f_sp;
    const double logstep = log(6.4) / 27.0;
    if (mel >= min_log_mel) {
        return min_log_hz * exp(logstep * (mel - min_log_mel));
    }
    return f_sp * mel;
}

/* 构建 80 x 201、按 Slaney 归一化的三角滤波器组。 */
static void build_filter_bank(float *filters) {
    double edges[NPUT_MEL_BINS + 2];
    const double mel_max = slaney_hz_to_mel((double)NPUT_MEL_RATE / 2.0);
    for (int i = 0; i < NPUT_MEL_BINS + 2; ++i) {
        const double mel = mel_max * (double)i / (double)(NPUT_MEL_BINS + 1);
        edges[i] = slaney_mel_to_hz(mel);
    }

    const double bin_width = ((double)NPUT_MEL_RATE / 2.0) / (double)(NPUT_MEL_BINS_PER_SPECTRUM - 1);
    for (int bin = 0; bin < NPUT_MEL_BINS; ++bin) {
        const double left = edges[bin];
        const double centre = edges[bin + 1];
        const double right = edges[bin + 2];
        const double norm = 2.0 / (right - left);
        for (int k = 0; k < NPUT_MEL_BINS_PER_SPECTRUM; ++k) {
            const double freq = (double)k * bin_width;
            const double rising = (freq - left) / (centre - left);
            const double falling = (right - freq) / (right - centre);
            double weight = rising < falling ? rising : falling;
            if (weight < 0.0 || !isfinite(weight)) {
                weight = 0.0;
            }
            filters[(size_t)bin * NPUT_MEL_BINS_PER_SPECTRUM + (size_t)k] = (float)(weight * norm);
        }
    }
}

static void build_windows(float *hann, float *cos_table, float *sin_table) {
    for (int n = 0; n < NPUT_MEL_WINDOW; ++n) {
        hann[n] = (float)(0.5 - 0.5 * cos(2.0 * M_PI * (double)n / (double)NPUT_MEL_WINDOW));
    }
    for (int k = 0; k < NPUT_MEL_BINS_PER_SPECTRUM; ++k) {
        for (int n = 0; n < NPUT_MEL_WINDOW; ++n) {
            const double angle = 2.0 * M_PI * (double)k * (double)n / (double)NPUT_MEL_WINDOW;
            cos_table[(size_t)k * NPUT_MEL_WINDOW + (size_t)n] = (float)cos(angle);
            sin_table[(size_t)k * NPUT_MEL_WINDOW + (size_t)n] = (float)sin(angle);
        }
    }
}

/* 与 torch.stft(center=True, pad_mode="reflect") 一致：先把音频补零到
   30 秒，再对 30 秒缓冲区做半个窗长的反射填充。 */
static void reflect_pad_30s(float *padded) {
    const size_t base = (size_t)NPUT_MEL_PAD;
    for (size_t i = 0; i < base; ++i) {
        padded[i] = padded[base + base - i];
    }
    for (size_t i = 0; i < base; ++i) {
        padded[base + (size_t)NPUT_MEL_SAMPLES + i] =
            padded[base + (size_t)NPUT_MEL_SAMPLES - 2 - i];
    }
}

nput_status nput_mel_compute(const float *samples, size_t count, nput_mel *out) {
    if (samples == NULL || out == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    out->data = NULL;
    out->bins = 0;
    out->frames = 0;

    float *padded = (float *)calloc((size_t)NPUT_MEL_PADDED, sizeof(float));
    float *filter_bank = (float *)malloc((size_t)NPUT_MEL_BINS * NPUT_MEL_BINS_PER_SPECTRUM * sizeof(float));
    float *hann = (float *)malloc((size_t)NPUT_MEL_WINDOW * sizeof(float));
    float *cos_table = (float *)malloc((size_t)NPUT_MEL_BINS_PER_SPECTRUM * NPUT_MEL_WINDOW * sizeof(float));
    float *sin_table = (float *)malloc((size_t)NPUT_MEL_BINS_PER_SPECTRUM * NPUT_MEL_WINDOW * sizeof(float));
    float *windowed = (float *)malloc((size_t)NPUT_MEL_WINDOW * sizeof(float));
    float *power = (float *)malloc((size_t)NPUT_MEL_BINS_PER_SPECTRUM * sizeof(float));
    float *mel = (float *)malloc((size_t)NPUT_MEL_BINS * NPUT_MEL_FRAMES * sizeof(float));

    if (padded == NULL || filter_bank == NULL || hann == NULL || cos_table == NULL ||
        sin_table == NULL || windowed == NULL || power == NULL || mel == NULL) {
        free(padded);
        free(filter_bank);
        free(hann);
        free(cos_table);
        free(sin_table);
        free(windowed);
        free(power);
        free(mel);
        return NPUT_ERR_OUT_OF_MEMORY;
    }

    const size_t usable = count > (size_t)NPUT_MEL_SAMPLES ? (size_t)NPUT_MEL_SAMPLES : count;
    memcpy(padded + NPUT_MEL_PAD, samples, usable * sizeof(float));
    reflect_pad_30s(padded);
    build_filter_bank(filter_bank);
    build_windows(hann, cos_table, sin_table);

    for (int frame = 0; frame < NPUT_MEL_FRAMES; ++frame) {
        const float *window = padded + (size_t)frame * NPUT_MEL_HOP;
        for (int n = 0; n < NPUT_MEL_WINDOW; ++n) {
            windowed[n] = window[n] * hann[n];
        }
        for (int k = 0; k < NPUT_MEL_BINS_PER_SPECTRUM; ++k) {
            const float *cos_row = cos_table + (size_t)k * NPUT_MEL_WINDOW;
            const float *sin_row = sin_table + (size_t)k * NPUT_MEL_WINDOW;
            float re = 0.0f;
            float im = 0.0f;
            for (int n = 0; n < NPUT_MEL_WINDOW; ++n) {
                re += windowed[n] * cos_row[n];
                im += windowed[n] * sin_row[n];
            }
            power[k] = re * re + im * im;
        }
        for (int bin = 0; bin < NPUT_MEL_BINS; ++bin) {
            const float *weights = filter_bank + (size_t)bin * NPUT_MEL_BINS_PER_SPECTRUM;
            float accumulator = 0.0f;
            for (int k = 0; k < NPUT_MEL_BINS_PER_SPECTRUM; ++k) {
                accumulator += weights[k] * power[k];
            }
            mel[(size_t)bin * NPUT_MEL_FRAMES + (size_t)frame] = accumulator;
        }
    }

    float maximum = -1e30f;
    const size_t total = (size_t)NPUT_MEL_BINS * NPUT_MEL_FRAMES;
    for (size_t i = 0; i < total; ++i) {
        const float floor_value = 1e-10f;
        float value = mel[i] > floor_value ? mel[i] : floor_value;
        value = log10f(value);
        mel[i] = value;
        if (value > maximum) {
            maximum = value;
        }
    }
    const float ceiling = maximum - 8.0f;
    for (size_t i = 0; i < total; ++i) {
        float value = mel[i] > ceiling ? mel[i] : ceiling;
        mel[i] = (value + 4.0f) / 4.0f;
    }

    free(padded);
    free(filter_bank);
    free(hann);
    free(cos_table);
    free(sin_table);
    free(windowed);
    free(power);

    out->data = mel;
    out->bins = NPUT_MEL_BINS;
    out->frames = NPUT_MEL_FRAMES;
    return NPUT_OK;
}

void nput_mel_free(nput_mel *mel) {
    if (mel == NULL) {
        return;
    }
    free(mel->data);
    mel->data = NULL;
    mel->bins = 0;
    mel->frames = 0;
}
