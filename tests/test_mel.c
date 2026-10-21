#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nput/mel.h"
#include "nput/status.h"
#include "unity.h"

/* 下列数值由 transformers 5.20.1 的 WhisperFeatureExtractor(feature_size=80)
   以 numpy 后端生成（return_tensors="np"），输入为 build_tone 生成的同一信号；
   未使用本仓库输出反向生成。 */
typedef struct mel_reference {
    int bin;
    int frame;
    float expected;
} mel_reference;

static const mel_reference REFERENCES[] = {
    {0, 0, 0.983279f},  {1, 0, 0.986621f},   {10, 1500, 1.348741f}, {79, 0, -0.081692f},
    {5, 10, -0.561789f}, {20, 100, -0.561789f}, {79, 2999, -0.561789f},
    {20, 1599, 0.179226f}, {20, 1600, 0.667385f}, {20, 1601, 0.179226f},
    {20, 1602, -0.561796f},
};

static const mel_reference SHORT_REFERENCES[] = {
    {0, 0, 0.971646f},  {20, 0, 0.823372f}, {79, 0, -0.057127f},
    {20, 1, 0.743316f}, {20, 10, -0.663211f}, {79, 2999, -0.663211f},
};

static void build_tone(float *samples, size_t count) {
    /* 生成覆盖整个缓冲区的 440 Hz 确定性单音；参考值来自同一条 16 秒信号。 */
    for (size_t i = 0; i < count; ++i) {
        const double angle = 2.0 * 3.14159265358979323846 * 440.0 * (double)i / 16000.0;
        samples[i] = (float)(0.5 * sin(angle));
    }
}

void setUp(void) {}
void tearDown(void) {}

static void test_mel_matches_reference(void) {
    const size_t count = 16 * 16000;
    float *samples = (float *)malloc(count * sizeof(float));
    TEST_ASSERT_NOT_NULL(samples);
    build_tone(samples, count);

    nput_mel mel;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_mel_compute(samples, count, &mel));
    TEST_ASSERT_EQUAL_size_t(NPUT_MEL_BINS, mel.bins);
    TEST_ASSERT_EQUAL_size_t(NPUT_MEL_FRAMES, mel.frames);

    for (size_t i = 0; i < sizeof(REFERENCES) / sizeof(REFERENCES[0]); ++i) {
        const float actual = mel.data[(size_t)REFERENCES[i].bin * NPUT_MEL_FRAMES +
                                      (size_t)REFERENCES[i].frame];
        TEST_ASSERT_FLOAT_WITHIN(0.002f, REFERENCES[i].expected, actual);
    }

    nput_mel_free(&mel);
    free(samples);
}

static void test_mel_rejects_null(void) {
    nput_mel mel;
    TEST_ASSERT_EQUAL(NPUT_ERR_INVALID_ARGUMENT, nput_mel_compute(NULL, 0, &mel));
}

static void test_mel_pads_short_input(void) {
    float samples[160];
    build_tone(samples, 160);
    nput_mel mel;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_mel_compute(samples, 160, &mel));
    TEST_ASSERT_EQUAL_size_t(NPUT_MEL_FRAMES, mel.frames);
    /* 尾部没有信号，因此落在钳位下限上。 */
    const float tail = mel.data[(size_t)79 * NPUT_MEL_FRAMES + 2999];
    TEST_ASSERT_TRUE(tail < -0.4f);
    nput_mel_free(&mel);
}

static void test_mel_short_input_matches_reference(void) {
    float samples[160];
    build_tone(samples, 160);
    nput_mel mel;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_mel_compute(samples, 160, &mel));
    for (size_t i = 0; i < sizeof(SHORT_REFERENCES) / sizeof(SHORT_REFERENCES[0]); ++i) {
        const float actual = mel.data[(size_t)SHORT_REFERENCES[i].bin * NPUT_MEL_FRAMES +
                                      (size_t)SHORT_REFERENCES[i].frame];
        TEST_ASSERT_FLOAT_WITHIN(0.002f, SHORT_REFERENCES[i].expected, actual);
    }
    nput_mel_free(&mel);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mel_matches_reference);
    RUN_TEST(test_mel_rejects_null);
    RUN_TEST(test_mel_pads_short_input);
    RUN_TEST(test_mel_short_input_matches_reference);
    return UNITY_END();
}
