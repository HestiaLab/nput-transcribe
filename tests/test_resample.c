#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "nput/audio.h"
#include "unity.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void setUp(void) {}
void tearDown(void) {}

static nput_audio make_tone(int rate, size_t count, double frequency) {
    nput_audio audio;
    nput_audio_init(&audio);
    audio.sample_rate = rate;
    audio.count = count;
    audio.samples = (float *)malloc(count * sizeof(float));
    for (size_t i = 0; i < count; ++i) {
        audio.samples[i] = (float)(0.5 * sin(2.0 * M_PI * frequency * (double)i / (double)rate));
    }
    return audio;
}

static void test_resample_downsample_length(void) {
    nput_audio source = make_tone(44100, 44100, 1000.0);
    nput_audio result;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_audio_resample(&source, 16000, &result));
    TEST_ASSERT_EQUAL_INT(16000, result.sample_rate);
    TEST_ASSERT_EQUAL_size_t(16000, result.count);
    nput_audio_free(&result);
    nput_audio_free(&source);
}

static void test_resample_preserves_amplitude(void) {
    /* 1 kHz 单音位于 44.1 kHz -> 16 kHz 的通带内。 */
    nput_audio source = make_tone(44100, 44100, 1000.0);
    nput_audio result;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_audio_resample(&source, 16000, &result));

    float peak = 0.0f;
    for (size_t i = 1000; i + 1000 < result.count; ++i) {
        const float magnitude = fabsf(result.samples[i]);
        if (magnitude > peak) {
            peak = magnitude;
        }
    }
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.5f, peak);
    nput_audio_free(&result);
    nput_audio_free(&source);
}

static void test_resample_upsample_length(void) {
    nput_audio source = make_tone(8000, 8000, 440.0);
    nput_audio result;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_audio_resample(&source, 16000, &result));
    TEST_ASSERT_EQUAL_size_t(16000, result.count);
    nput_audio_free(&result);
    nput_audio_free(&source);
}

static void test_resample_same_rate_copies(void) {
    nput_audio source = make_tone(16000, 1000, 440.0);
    nput_audio result;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_audio_resample(&source, 16000, &result));
    TEST_ASSERT_EQUAL_size_t(source.count, result.count);
    TEST_ASSERT_EQUAL_MEMORY(source.samples, result.samples, source.count * sizeof(float));
    TEST_ASSERT_TRUE(source.samples != result.samples);
    nput_audio_free(&result);
    nput_audio_free(&source);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_resample_downsample_length);
    RUN_TEST(test_resample_preserves_amplitude);
    RUN_TEST(test_resample_upsample_length);
    RUN_TEST(test_resample_same_rate_copies);
    return UNITY_END();
}
