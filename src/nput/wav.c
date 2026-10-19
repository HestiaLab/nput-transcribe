#include "nput/internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define NPUT_FSEEK _fseeki64
#define NPUT_FTELL _ftelli64
#else
#define NPUT_FSEEK fseeko
#define NPUT_FTELL ftello
#endif

enum {
    NPUT_WAV_TAG_PCM = 1,
    NPUT_WAV_TAG_FLOAT = 3,
    NPUT_WAV_TAG_EXTENSIBLE = 0xFFFE,
    NPUT_WAV_MAX_CHANNELS = 8
};

static uint16_t read_u16le(const unsigned char *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static float to_float(const unsigned char *p, uint16_t bits, uint16_t tag) {
    if (tag == NPUT_WAV_TAG_FLOAT) {
        if (bits == 32) {
            float value;
            memcpy(&value, p, sizeof(value));
            return value;
        }
        if (bits == 64) {
            double value;
            memcpy(&value, p, sizeof(value));
            return (float)value;
        }
        return 0.0f;
    }
    switch (bits) {
        case 8:
            return ((float)p[0] - 128.0f) / 128.0f;
        case 16: {
            int16_t value;
            memcpy(&value, p, sizeof(value));
            return (float)value / 32768.0f;
        }
        case 24: {
            int32_t value = (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16));
            if ((value & 0x800000) != 0) {
                value -= 0x1000000;
            }
            return (float)value / 8388608.0f;
        }
        case 32: {
            int32_t value;
            memcpy(&value, p, sizeof(value));
            return (float)((double)value / 2147483648.0);
        }
        default:
            return 0.0f;
    }
}

static nput_status append_samples(float **samples, size_t *count, size_t *capacity, const float *src,
                                  size_t src_count) {
    if (*count + src_count > *capacity) {
        size_t wanted = *capacity == 0 ? 65536 : *capacity;
        while (wanted < *count + src_count) {
            wanted *= 2;
        }
        float *grown = (float *)realloc(*samples, wanted * sizeof(float));
        if (grown == NULL) {
            return NPUT_ERR_OUT_OF_MEMORY;
        }
        *samples = grown;
        *capacity = wanted;
    }
    memcpy(*samples + *count, src, src_count * sizeof(float));
    *count += src_count;
    return NPUT_OK;
}

nput_status nput_wav_decode(const char *path, nput_audio *out) {
    if (path == NULL || out == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    nput_audio_init(out);

    FILE *file = nput_file_open(path, "rb");
    if (file == NULL) {
        return NPUT_ERR_IO;
    }

    unsigned char header[12];
    if (fread(header, 1, sizeof(header), file) != sizeof(header) || memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0) {
        fclose(file);
        return NPUT_ERR_FORMAT;
    }

    uint16_t tag = 0;
    uint16_t channels = 0;
    uint16_t bits = 0;
    uint32_t rate = 0;
    int have_format = 0;
    int have_data = 0;
    int64_t data_offset = 0;
    int64_t data_size = 0;

    /* 逐个读取 RIFF 块：跳过未知块，拿到 fmt 与 data 后停止。 */
    while (!have_data) {
        unsigned char chunk[8];
        if (fread(chunk, 1, sizeof(chunk), file) != sizeof(chunk)) {
            break;
        }
        const uint32_t size = read_u32le(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char format[40];
            size_t to_read = size > sizeof(format) ? sizeof(format) : size;
            if (fread(format, 1, to_read, file) != to_read) {
                break;
            }
            if (size > to_read) {
                NPUT_FSEEK(file, (int64_t)(size - to_read), SEEK_CUR);
            }
            tag = read_u16le(format);
            channels = read_u16le(format + 2);
            rate = read_u32le(format + 4);
            bits = read_u16le(format + 14);
            if (tag == NPUT_WAV_TAG_EXTENSIBLE && to_read >= 26) {
                tag = read_u16le(format + 24);
            }
            have_format = 1;
        } else if (memcmp(chunk, "data", 4) == 0) {
            data_offset = NPUT_FTELL(file);
            data_size = (int64_t)size;
            have_data = 1;
        } else {
            NPUT_FSEEK(file, (int64_t)size + (int64_t)(size & 1u), SEEK_CUR);
        }
    }

    if (!have_format || !have_data) {
        fclose(file);
        return NPUT_ERR_FORMAT;
    }
    if (channels == 0 || channels > NPUT_WAV_MAX_CHANNELS || rate == 0) {
        fclose(file);
        return NPUT_ERR_FORMAT;
    }
    if (tag != NPUT_WAV_TAG_PCM && tag != NPUT_WAV_TAG_FLOAT) {
        fclose(file);
        return NPUT_ERR_FORMAT;
    }
    if (bits != 8 && bits != 16 && bits != 24 && bits != 32 && bits != 64) {
        fclose(file);
        return NPUT_ERR_FORMAT;
    }
    if (tag == NPUT_WAV_TAG_FLOAT && bits != 32 && bits != 64) {
        fclose(file);
        return NPUT_ERR_FORMAT;
    }

    const size_t frame_bytes = (size_t)channels * (bits / 8u);
    size_t capacity = 0;
    size_t count = 0;
    float *samples = NULL;
    nput_status status = NPUT_OK;

    if (NPUT_FSEEK(file, data_offset, SEEK_SET) != 0) {
        fclose(file);
        return NPUT_ERR_IO;
    }

    /* 按 64 KB 分块读取并立即混为单声道，避免整个文件驻留内存。 */
    const size_t block_bytes = ((size_t)1 << 16) / frame_bytes * frame_bytes;
    unsigned char *block = (unsigned char *)malloc(block_bytes);
    float *frames = (float *)malloc((block_bytes / frame_bytes) * sizeof(float));
    if (block == NULL || frames == NULL) {
        status = NPUT_ERR_OUT_OF_MEMORY;
    }

    int64_t remaining = data_size;
    while (status == NPUT_OK && remaining > 0) {
        size_t wanted = block_bytes;
        if ((int64_t)wanted > remaining) {
            wanted = (size_t)remaining;
        }
        wanted -= wanted % frame_bytes;
        if (wanted == 0) {
            break;
        }
        const size_t got = fread(block, 1, wanted, file);
        if (got < frame_bytes) {
            break;
        }
        const size_t frames_read = got / frame_bytes;
        for (size_t i = 0; i < frames_read; ++i) {
            const unsigned char *frame = block + i * frame_bytes;
            float sum = 0.0f;
            for (size_t c = 0; c < channels; ++c) {
                sum += to_float(frame + c * (bits / 8u), bits, tag);
            }
            frames[i] = sum / (float)channels;
        }
        status = append_samples(&samples, &count, &capacity, frames, frames_read);
        remaining -= (int64_t)got;
    }

    free(block);
    free(frames);
    fclose(file);

    if (status != NPUT_OK) {
        free(samples);
        return status;
    }

    out->samples = samples;
    out->count = count;
    out->sample_rate = (int)rate;
    return NPUT_OK;
}
