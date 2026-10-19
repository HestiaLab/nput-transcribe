#include <stdlib.h>
#include <string.h>

#include "nput/segment.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_short_audio_single_segment(void) {
    float samples[1000];
    memset(samples, 0, sizeof(samples));
    nput_segment *segments = NULL;
    size_t count = 0;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_segment_split(samples, 1000, 16000, 480000, &segments, &count));
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_size_t(0, segments[0].begin);
    TEST_ASSERT_EQUAL_size_t(1000, segments[0].end);
    nput_segments_free(segments);
}

static void test_empty_audio(void) {
    nput_segment *segments = NULL;
    size_t count = 0;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_segment_split(NULL, 0, 16000, 480000, &segments, &count));
    TEST_ASSERT_EQUAL_size_t(0, count);
    nput_segments_free(segments);
}

static void test_splits_without_gaps_and_within_limit(void) {
    /* 10 秒语音，中间夹 2 秒静音。 */
    const size_t rate = 16000;
    const size_t total = 10 * rate;
    float *samples = (float *)malloc(total * sizeof(float));
    for (size_t i = 0; i < total; ++i) {
        const int silent = i >= 4 * rate && i < 6 * rate;
        samples[i] = silent ? 0.0f : 0.3f;
    }

    nput_segment *segments = NULL;
    size_t count = 0;
    const size_t limit = 5 * rate;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_segment_split(samples, total, (int)rate, limit, &segments, &count));
    TEST_ASSERT_TRUE(count >= 2);

    TEST_ASSERT_EQUAL_size_t(0, segments[0].begin);
    TEST_ASSERT_EQUAL_size_t(total, segments[count - 1].end);
    for (size_t i = 0; i < count; ++i) {
        TEST_ASSERT_TRUE(segments[i].end > segments[i].begin);
        TEST_ASSERT_TRUE(segments[i].end - segments[i].begin <= limit);
        if (i > 0) {
            TEST_ASSERT_EQUAL_size_t(segments[i - 1].end, segments[i].begin);
        }
    }

    nput_segments_free(segments);
    free(samples);
}

static void test_cut_lands_in_silence(void) {
    const size_t rate = 16000;
    const size_t total = 8 * rate;
    float *samples = (float *)malloc(total * sizeof(float));
    for (size_t i = 0; i < total; ++i) {
        /* 4 到 5 秒之间是静音，恰好是切分上限所在位置。 */
        const int silent = i >= 4 * rate && i < 5 * rate;
        samples[i] = silent ? 0.0f : 0.25f;
    }

    nput_segment *segments = NULL;
    size_t count = 0;
    TEST_ASSERT_EQUAL(NPUT_OK,
                      nput_segment_split(samples, total, (int)rate, 5 * rate, &segments, &count));
    TEST_ASSERT_TRUE(count >= 2);
    TEST_ASSERT_TRUE(segments[0].end >= 4 * rate);
    TEST_ASSERT_TRUE(segments[0].end <= 5 * rate);

    nput_segments_free(segments);
    free(samples);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_short_audio_single_segment);
    RUN_TEST(test_empty_audio);
    RUN_TEST(test_splits_without_gaps_and_within_limit);
    RUN_TEST(test_cut_lands_in_silence);
    return UNITY_END();
}
