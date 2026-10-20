#include "nput/asr.h"

#include <stdlib.h>
#include <string.h>

#include "nput/backend.h"

enum {
    NPUT_ASR_BACKEND_CAPACITY = 8,
    /* 公共选项的默认解码上限；各后端可自行解释。 */
    NPUT_ASR_DEFAULT_MAX_TOKENS = 224
};

static const nput_asr_backend *backends[NPUT_ASR_BACKEND_CAPACITY] = {
    &nput_asr_backend_whisper,
};
static size_t backend_count = 1;

struct nput_asr {
    const nput_asr_backend *backend;
    void *state;
    nput_asr_timings timings;
};

static int backend_is_complete(const nput_asr_backend *backend) {
    return backend != NULL && backend->name != NULL && backend->name[0] != '\0' &&
           backend->create != NULL && backend->detect_language != NULL &&
           backend->transcribe != NULL && backend->destroy != NULL;
}

const nput_asr_backend *nput_asr_backend_find(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return backends[0];
    }
    for (size_t i = 0; i < backend_count; ++i) {
        if (strcmp(backends[i]->name, name) == 0) {
            return backends[i];
        }
    }
    return NULL;
}

nput_status nput_asr_backend_register(const nput_asr_backend *backend) {
    if (!backend_is_complete(backend)) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < backend_count; ++i) {
        if (strcmp(backends[i]->name, backend->name) == 0) {
            backends[i] = backend;
            return NPUT_OK;
        }
    }
    if (backend_count >= NPUT_ASR_BACKEND_CAPACITY) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    backends[backend_count++] = backend;
    return NPUT_OK;
}

void nput_asr_options_init(nput_asr_options *options) {
    if (options == NULL) {
        return;
    }
    memset(options, 0, sizeof(*options));
    options->encoder_device = "NPU";
    options->decoder_device = "CPU";
    options->max_tokens = NPUT_ASR_DEFAULT_MAX_TOKENS;
}

nput_status nput_asr_create(const nput_asr_options *options, nput_asr **out_asr) {
    if (options == NULL || out_asr == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_asr = NULL;

    const nput_asr_backend *backend = nput_asr_backend_find(options->backend);
    if (backend == NULL) {
        return NPUT_ERR_UNSUPPORTED;
    }

    nput_asr *asr = (nput_asr *)calloc(1, sizeof(*asr));
    if (asr == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    asr->backend = backend;

    const nput_status status = backend->create(options, &asr->state);
    if (status != NPUT_OK) {
        free(asr);
        return status;
    }
    *out_asr = asr;
    return NPUT_OK;
}

void nput_asr_destroy(nput_asr *asr) {
    if (asr == NULL) {
        return;
    }
    asr->backend->destroy(asr->state);
    free(asr);
}

nput_status nput_asr_detect_language(nput_asr *asr, const float *samples, size_t count,
                                     int32_t *out_token) {
    if (asr == NULL || out_token == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    return asr->backend->detect_language(asr->state, samples, count, out_token);
}

nput_status nput_asr_transcribe(nput_asr *asr, const float *samples, size_t count,
                                int32_t language_token, char **out_text) {
    if (asr == NULL || out_text == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;

    nput_asr_timings timings = asr->timings;
    const nput_status status =
        asr->backend->transcribe(asr->state, samples, count, language_token, out_text, &timings);
    asr->timings = timings;
    return status;
}

void nput_asr_last_timings(const nput_asr *asr, nput_asr_timings *out_timings) {
    if (asr == NULL || out_timings == NULL) {
        return;
    }
    *out_timings = asr->timings;
}