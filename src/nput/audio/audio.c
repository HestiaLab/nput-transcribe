#include "nput/audio.h"

#include "audio/decode.h"

#include <stdlib.h>
#include <string.h>

void nput_audio_init(nput_audio *audio) {
    if (audio == NULL) {
        return;
    }
    audio->samples = NULL;
    audio->count = 0;
    audio->sample_rate = 0;
}

void nput_audio_free(nput_audio *audio) {
    if (audio == NULL) {
        return;
    }
    free(audio->samples);
    nput_audio_init(audio);
}

nput_status nput_audio_load_wav(const char *path, nput_audio *out) {
    return nput_wav_decode(path, out);
}

static int has_extension(const char *path, const char *extension) {
    const size_t path_length = strlen(path);
    const size_t extension_length = strlen(extension);
    if (path_length < extension_length) {
        return 0;
    }
    const char *tail = path + path_length - extension_length;
    for (size_t i = 0; i < extension_length; ++i) {
        char a = tail[i];
        char b = extension[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

nput_status nput_audio_load(const char *path, nput_audio *out) {
    if (path == NULL || out == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    if (has_extension(path, ".wav") || has_extension(path, ".wave")) {
        return nput_wav_decode(path, out);
    }
    return nput_platform_decode(path, out);
}
