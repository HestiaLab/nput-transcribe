#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nput/asr.h"
#include "asr/backend.h"
#include "unity.h"

enum { STUB_LANGUAGE_BASE = 4242 };

typedef struct stub_state {
    int created;
} stub_state;

static nput_status stub_create(const nput_asr_options *options, void **out_state) {
    (void)options;
    stub_state *state = (stub_state *)calloc(1, sizeof(*state));
    if (state == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    state->created = 1;
    *out_state = state;
    return NPUT_OK;
}

static nput_status stub_detect_language(void *state, const float *samples, size_t count,
                                        int32_t *out_token) {
    stub_state *stub = (stub_state *)state;
    if (stub == NULL || out_token == NULL || (samples == NULL && count > 0)) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_token = STUB_LANGUAGE_BASE + (int32_t)(count % 7);
    return NPUT_OK;
}

static nput_status stub_transcribe(void *state, const float *samples, size_t count,
                                   int32_t language_token, char **out_text,
                                   nput_asr_timings *out_timings) {
    stub_state *stub = (stub_state *)state;
    if (stub == NULL || out_text == NULL || out_timings == NULL || (samples == NULL && count > 0)) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }

    char buffer[64];
    const int written = snprintf(buffer, sizeof(buffer), "stub:%zu:%d", count, (int)language_token);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return NPUT_ERR_RUNTIME;
    }
    *out_text = (char *)malloc((size_t)written + 1);
    if (*out_text == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    memcpy(*out_text, buffer, (size_t)written + 1);

    out_timings->encoder_ms = 1.5;
    out_timings->decoder_ms = 2.5;
    out_timings->tokens = count;
    return NPUT_OK;
}

static void stub_destroy(void *state) {
    free(state);
}

static const nput_asr_backend STUB_BACKEND = {
    "stub", stub_create, stub_detect_language, stub_transcribe, stub_destroy,
};

void setUp(void) {
    TEST_ASSERT_EQUAL(NPUT_OK, nput_asr_backend_register(&STUB_BACKEND));
}

void tearDown(void) {}

static nput_asr *create_stub(void) {
    nput_asr_options options;
    nput_asr_options_init(&options);
    options.backend = "stub";
    nput_asr *asr = NULL;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_asr_create(&options, &asr));
    TEST_ASSERT_NOT_NULL(asr);
    return asr;
}

static void test_stub_backend_does_not_need_model_dir(void) {
    nput_asr *asr = create_stub();
    nput_asr_destroy(asr);
}

static void test_stub_backend_transcribes_samples(void) {
    nput_asr *asr = create_stub();
    const float samples[4] = {0.0f, 0.25f, -0.25f, 1.0f};
    char *text = NULL;

    TEST_ASSERT_EQUAL(NPUT_OK, nput_asr_transcribe(asr, samples, 4, 50260, &text));
    TEST_ASSERT_EQUAL_STRING("stub:4:50260", text);

    nput_asr_timings timings;
    nput_asr_last_timings(asr, &timings);
    /* 桩值由字面量直接写入，未做运算；用精确比较即可，无需启用 Unity 浮点断言。 */
    TEST_ASSERT_TRUE(timings.encoder_ms == 1.5);
    TEST_ASSERT_TRUE(timings.decoder_ms == 2.5);
    TEST_ASSERT_EQUAL_size_t(4, timings.tokens);

    nput_free(text);
    nput_asr_destroy(asr);
}

static void test_stub_backend_detects_language(void) {
    nput_asr *asr = create_stub();
    const float samples[10] = {0};
    int32_t token = -1;

    TEST_ASSERT_EQUAL(NPUT_OK, nput_asr_detect_language(asr, samples, 10, &token));
    TEST_ASSERT_EQUAL_INT32(STUB_LANGUAGE_BASE + 3, token);

    nput_asr_destroy(asr);
}

static void test_unknown_backend_is_rejected(void) {
    nput_asr_options options;
    nput_asr_options_init(&options);
    options.backend = "missing";
    nput_asr *asr = NULL;

    TEST_ASSERT_EQUAL(NPUT_ERR_UNSUPPORTED, nput_asr_create(&options, &asr));
    TEST_ASSERT_NULL(asr);
}

static void test_default_backend_requires_model_dir(void) {
    nput_asr_options options;
    nput_asr_options_init(&options);
    TEST_ASSERT_NULL(options.backend);
    nput_asr *asr = NULL;

    TEST_ASSERT_EQUAL(NPUT_ERR_INVALID_ARGUMENT, nput_asr_create(&options, &asr));
    TEST_ASSERT_NULL(asr);
}

static void test_transcribe_rejects_null_arguments(void) {
    nput_asr *asr = create_stub();
    const float samples[1] = {0.0f};
    char *text = NULL;

    TEST_ASSERT_EQUAL(NPUT_ERR_INVALID_ARGUMENT, nput_asr_transcribe(NULL, samples, 1, -1, &text));
    TEST_ASSERT_EQUAL(NPUT_ERR_INVALID_ARGUMENT, nput_asr_transcribe(asr, samples, 1, -1, NULL));
    TEST_ASSERT_EQUAL(NPUT_ERR_INVALID_ARGUMENT, nput_asr_transcribe(asr, NULL, 1, -1, &text));
    TEST_ASSERT_NULL(text);

    int32_t token = -1;
    TEST_ASSERT_EQUAL(NPUT_ERR_INVALID_ARGUMENT, nput_asr_detect_language(asr, NULL, 1, &token));
    TEST_ASSERT_EQUAL(NPUT_ERR_INVALID_ARGUMENT, nput_asr_detect_language(asr, samples, 1, NULL));

    nput_asr_destroy(asr);
}

static void test_incomplete_backend_is_rejected(void) {
    const nput_asr_backend incomplete = {
        "incomplete", stub_create, stub_detect_language, NULL, stub_destroy,
    };
    TEST_ASSERT_EQUAL(NPUT_ERR_INVALID_ARGUMENT, nput_asr_backend_register(&incomplete));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_stub_backend_does_not_need_model_dir);
    RUN_TEST(test_stub_backend_transcribes_samples);
    RUN_TEST(test_stub_backend_detects_language);
    RUN_TEST(test_unknown_backend_is_rejected);
    RUN_TEST(test_default_backend_requires_model_dir);
    RUN_TEST(test_transcribe_rejects_null_arguments);
    RUN_TEST(test_incomplete_backend_is_rejected);
    return UNITY_END();
}
