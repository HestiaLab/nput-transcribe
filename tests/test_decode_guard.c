#include <stdint.h>

#include "text/trigram.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_no_guard_with_short_history(void) {
    const int64_t ids[] = {7};
    TEST_ASSERT_EQUAL_INT(0, nput_trigram_repeats(ids, 1, 7));
    TEST_ASSERT_EQUAL_INT(0, nput_trigram_repeats(NULL, 0, 7));
}

static void test_first_occurrence_allowed(void) {
    const int64_t ids[] = {1, 2};
    TEST_ASSERT_EQUAL_INT(0, nput_trigram_repeats(ids, 2, 3));
}

static void test_repeated_trigram_blocked(void) {
    /* 已出现 1 2 3，因此在 1 2 之后再提出 3 必须被拒绝。 */
    const int64_t ids[] = {1, 2, 3, 4, 1, 2};
    TEST_ASSERT_EQUAL_INT(1, nput_trigram_repeats(ids, 6, 3));
    TEST_ASSERT_EQUAL_INT(0, nput_trigram_repeats(ids, 6, 5));
}

static void test_only_the_same_pair_is_blocked(void) {
    const int64_t ids[] = {1, 2, 3, 2, 3};
    /* 当前末尾是 (2, 3)；历史在索引 1 处已有 (2, 3, 2)。 */
    TEST_ASSERT_EQUAL_INT(1, nput_trigram_repeats(ids, 5, 2));
    TEST_ASSERT_EQUAL_INT(0, nput_trigram_repeats(ids, 5, 1));
    TEST_ASSERT_EQUAL_INT(0, nput_trigram_repeats(ids, 5, 4));
}

static void test_long_run_collapses(void) {
    /* 复现音乐循环：9 9 9……必须在第三次重复时停止。 */
    int64_t ids[8];
    size_t count = 0;
    for (size_t i = 0; i < 8; ++i) {
        if (i < 3) {
            ids[count++] = 9;
            continue;
        }
        if (nput_trigram_repeats(ids, count, 9)) {
            break;
        }
        ids[count++] = 9;
    }
    TEST_ASSERT_EQUAL_size_t(3, count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_guard_with_short_history);
    RUN_TEST(test_first_occurrence_allowed);
    RUN_TEST(test_repeated_trigram_blocked);
    RUN_TEST(test_only_the_same_pair_is_blocked);
    RUN_TEST(test_long_run_collapses);
    return UNITY_END();
}
