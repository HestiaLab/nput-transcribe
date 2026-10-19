#include "nput/internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

static nput_status append_mono(float **samples, size_t *count, size_t *capacity, const int16_t *pcm,
                              size_t frames, uint16_t channels) {
    if (*count + frames > *capacity) {
        size_t wanted = *capacity == 0 ? 65536 : *capacity;
        while (wanted < *count + frames) {
            wanted *= 2;
        }
        float *grown = (float *)realloc(*samples, wanted * sizeof(float));
        if (grown == NULL) {
            return NPUT_ERR_OUT_OF_MEMORY;
        }
        *samples = grown;
        *capacity = wanted;
    }
    for (size_t i = 0; i < frames; ++i) {
        int32_t sum = 0;
        for (uint16_t c = 0; c < channels; ++c) {
            sum += pcm[i * channels + c];
        }
        (*samples)[*count + i] = (float)sum / ((float)channels * 32768.0f);
    }
    *count += frames;
    return NPUT_OK;
}

nput_status nput_platform_decode(const char *path, nput_audio *out) {
    if (path == NULL || out == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    nput_audio_init(out);

    wchar_t wide_path[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, 1024) == 0) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }

    const HRESULT com_status = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    const int com_owned = SUCCEEDED(com_status) && com_status != S_FALSE;

    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_FULL))) {
        if (com_owned) {
            CoUninitialize();
        }
        return NPUT_ERR_RUNTIME;
    }

    nput_status status = NPUT_OK;
    IMFSourceReader *reader = NULL;
    IMFMediaType *output_type = NULL;
    IMFMediaType *actual_type = NULL;
    float *samples = NULL;
    size_t count = 0;
    size_t capacity = 0;
    uint16_t channels = 0;
    uint32_t rate = 0;

    if (FAILED(MFCreateSourceReaderFromURL(wide_path, NULL, &reader))) {
        status = NPUT_ERR_IO;
        goto done;
    }
    if (FAILED(MFCreateMediaType(&output_type)) ||
        FAILED(output_type->lpVtbl->SetGUID(output_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio)) ||
        FAILED(output_type->lpVtbl->SetGUID(output_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM))) {
        status = NPUT_ERR_RUNTIME;
        goto done;
    }
    if (FAILED(reader->lpVtbl->SetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL,
                                                  output_type))) {
        status = NPUT_ERR_UNSUPPORTED;
        goto done;
    }
    if (FAILED(reader->lpVtbl->GetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                                  &actual_type))) {
        status = NPUT_ERR_RUNTIME;
        goto done;
    }

    UINT32 value = 0;
    if (SUCCEEDED(actual_type->lpVtbl->GetUINT32(actual_type, &MF_MT_AUDIO_NUM_CHANNELS, &value))) {
        channels = (uint16_t)value;
    }
    if (SUCCEEDED(actual_type->lpVtbl->GetUINT32(actual_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &value))) {
        rate = value;
    }
    UINT32 bits = 0;
    if (SUCCEEDED(actual_type->lpVtbl->GetUINT32(actual_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &bits))) {
        if (bits != 16) {
            status = NPUT_ERR_UNSUPPORTED;
            goto done;
        }
    }
    if (channels == 0 || channels > 8 || rate == 0) {
        status = NPUT_ERR_FORMAT;
        goto done;
    }

    int empty_reads = 0;

    for (;;) {
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample *sample = NULL;
        if (FAILED(reader->lpVtbl->ReadSample(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL,
                                             &flags, &timestamp, &sample))) {
            status = NPUT_ERR_RUNTIME;
            break;
        }
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
            if (sample != NULL) {
                sample->lpVtbl->Release(sample);
            }
            break;
        }
        if (sample == NULL) {
            /* 流时钟与媒体类型切换不会产生负载；限制重试次数，避免解码器
               不上报流结束时死循环。 */
            if (++empty_reads > 1024) {
                status = NPUT_ERR_UNSUPPORTED;
                break;
            }
            continue;
        }
        empty_reads = 0;
        IMFMediaBuffer *buffer = NULL;
        if (FAILED(sample->lpVtbl->ConvertToContiguousBuffer(sample, &buffer))) {
            sample->lpVtbl->Release(sample);
            status = NPUT_ERR_RUNTIME;
            break;
        }
        BYTE *data = NULL;
        DWORD length = 0;
        if (FAILED(buffer->lpVtbl->Lock(buffer, &data, NULL, &length))) {
            buffer->lpVtbl->Release(buffer);
            sample->lpVtbl->Release(sample);
            status = NPUT_ERR_RUNTIME;
            break;
        }
        const size_t frame_count = length / (sizeof(int16_t) * (size_t)channels);
        status = append_mono(&samples, &count, &capacity, (const int16_t *)data, frame_count, channels);
        buffer->lpVtbl->Unlock(buffer);
        buffer->lpVtbl->Release(buffer);
        sample->lpVtbl->Release(sample);
        if (status != NPUT_OK) {
            break;
        }
    }

done:
    if (actual_type != NULL) {
        actual_type->lpVtbl->Release(actual_type);
    }
    if (output_type != NULL) {
        output_type->lpVtbl->Release(output_type);
    }
    if (reader != NULL) {
        reader->lpVtbl->Release(reader);
    }
    MFShutdown();
    if (com_owned) {
        CoUninitialize();
    }

    if (status != NPUT_OK) {
        free(samples);
        return status;
    }

    out->samples = samples;
    out->count = count;
    out->sample_rate = (int)rate;
    return NPUT_OK;
}

#else

nput_status nput_platform_decode(const char *path, nput_audio *out) {
    (void)path;
    (void)out;
    return NPUT_ERR_UNSUPPORTED;
}

#endif


