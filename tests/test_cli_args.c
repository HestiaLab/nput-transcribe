#include <stdint.h>

#include "cli_args.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void expect_accept(const char *text, int32_t expected) {
    int32_t value = 0;
    TEST_ASSERT_TRUE(cli_parse_int(text, &value));
    TEST_ASSERT_EQUAL_INT32(expected, value);
}

static void expect_reject(const char *text) {
    int32_t value = 0;
    TEST_ASSERT_FALSE(cli_parse_int(text, &value));
}

static void test_accepts_valid_values(void) {
    expect_accept("1", 1);
    expect_accept("224", 224);
    expect_accept("2147483647", INT32_MAX);
}

static void test_rejects_out_of_range(void) {
    expect_reject("0");
    expect_reject("-1");
    expect_reject("2147483648");
    expect_reject("99999999999999999999");
}

static void test_rejects_malformed(void) {
    expect_reject("");
    expect_reject("abc");
    expect_reject("12x");
    expect_reject("1.5");
}

static void test_rejects_null_arguments(void) {
    int32_t value = 0;
    TEST_ASSERT_FALSE(cli_parse_int(NULL, &value));
    TEST_ASSERT_FALSE(cli_parse_int("1", NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_accepts_valid_values);
    RUN_TEST(test_rejects_out_of_range);
    RUN_TEST(test_rejects_malformed);
    RUN_TEST(test_rejects_null_arguments);
    return UNITY_END();
}
