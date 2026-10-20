#include <string.h>

#include "core/error.h"
#include "nput/status.h"
#include "unity.h"

void setUp(void) {
    nput_clear_error();
}

void tearDown(void) {
    nput_clear_error();
}

static void test_error_is_empty_by_default(void) {
    TEST_ASSERT_EQUAL_STRING("", nput_last_error());
}

static void test_error_records_formatted_message(void) {
    nput_set_error("OpenVINO：%s 失败（状态码 %d）", "读取编码器模型", 7);
    TEST_ASSERT_EQUAL_STRING("OpenVINO：读取编码器模型 失败（状态码 7）", nput_last_error());
}

static void test_error_truncates_without_overflow(void) {
    char text[600];
    memset(text, 'x', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';

    nput_set_error("%s", text);
    TEST_ASSERT_EQUAL_size_t(511, strlen(nput_last_error()));
}

static void test_clear_removes_message(void) {
    nput_set_error("temporary");
    nput_clear_error();
    TEST_ASSERT_EQUAL_STRING("", nput_last_error());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_error_is_empty_by_default);
    RUN_TEST(test_error_records_formatted_message);
    RUN_TEST(test_error_truncates_without_overflow);
    RUN_TEST(test_clear_removes_message);
    return UNITY_END();
}
