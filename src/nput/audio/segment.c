#include "nput/segment.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

enum {
    NPUT_SEGMENT_FRAMES_PER_SECOND = 50, /* 20 ms 分析帧 */
    NPUT_SEGMENT_SEARCH_SECONDS = 2,
    NPUT_SEGMENT_MIN_SECONDS = 2
};

nput_status nput_segment_split(const float *samples, size_t count, int sample_rate, size_t max_samples,
                               nput_segment **out_segments, size_t *out_count) {
    if (out_segments == NULL || out_count == NULL || sample_rate <= 0 || max_samples == 0) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    if (count > 0 && samples == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_segments = NULL;
    *out_count = 0;

    size_t capacity = 4;
    nput_segment *segments = (nput_segment *)malloc(capacity * sizeof(nput_segment));
    if (segments == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }

    const size_t frame = (size_t)sample_rate / NPUT_SEGMENT_FRAMES_PER_SECOND > 0
                             ? (size_t)sample_rate / NPUT_SEGMENT_FRAMES_PER_SECOND
                             : 1;
    const size_t frame_count = (count + frame - 1) / frame;
    const size_t search = (size_t)sample_rate * NPUT_SEGMENT_SEARCH_SECONDS;
    const size_t minimum = (size_t)sample_rate * NPUT_SEGMENT_MIN_SECONDS;

    size_t position = 0;
    while (position < count) {
        const size_t limit = position + max_samples;
        size_t end = count;
        if (limit < count) {
            /* 在上限前的最后两秒内寻找最安静的 20 ms 帧。 */
            size_t window_begin = limit > search ? limit - search : position + minimum;
            if (window_begin <= position) {
                window_begin = position + minimum;
            }
            size_t best_frame = limit / frame;
            double best_energy = 1e30;
            const size_t first_frame = window_begin / frame;
            const size_t last_frame = limit / frame;
            for (size_t f = first_frame; f < last_frame && f < frame_count; ++f) {
                double energy = 0.0;
                const size_t begin = f * frame;
                const size_t finish = begin + frame < count ? begin + frame : count;
                for (size_t i = begin; i < finish; ++i) {
                    energy += (double)samples[i] * (double)samples[i];
                }
                if (energy < best_energy) {
                    best_energy = energy;
                    best_frame = f;
                }
            }
            end = best_frame * frame;
            if (end <= position + minimum) {
                end = limit;
            }
            if (end > count) {
                end = count;
            }
        }

        if (position < end) {
            if (*out_count == capacity) {
                capacity *= 2;
                nput_segment *grown = (nput_segment *)realloc(segments, capacity * sizeof(nput_segment));
                if (grown == NULL) {
                    free(segments);
                    return NPUT_ERR_OUT_OF_MEMORY;
                }
                segments = grown;
            }
            segments[*out_count].begin = position;
            segments[*out_count].end = end;
            ++(*out_count);
        }

        if (end <= position) {
            break;
        }
        position = end;
    }

    *out_segments = segments;
    return NPUT_OK;
}

void nput_segments_free(nput_segment *segments) {
    free(segments);
}
