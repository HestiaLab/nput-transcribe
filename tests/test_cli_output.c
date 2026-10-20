#include <stdio.h>
#include <string.h>

#include "cli_output.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static FILE *open_capture(void) {
    FILE *file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    return file;
}

static void read_capture(FILE *file, char *buffer, size_t size) {
    TEST_ASSERT_TRUE(fflush(file) == 0);
    rewind(file);
    const size_t read = fread(buffer, 1, size - 1, file);
    buffer[read] = '\0';
}

static void test_timestamp_formats(void) {
    char buffer[32];
    cli_format_timestamp(buffer, sizeof(buffer), 0.0);
    TEST_ASSERT_EQUAL_STRING("00:00:00,000", buffer);
    cli_format_timestamp(buffer, sizeof(buffer), 3661.5);
    TEST_ASSERT_EQUAL_STRING("01:01:01,500", buffer);
    cli_format_timestamp(buffer, sizeof(buffer), -2.0);
    TEST_ASSERT_EQUAL_STRING("00:00:00,000", buffer);
    cli_format_timestamp(buffer, sizeof(buffer), 0.9996);
    TEST_ASSERT_EQUAL_STRING("00:00:01,000", buffer);
}

static void test_json_escape_control_characters(void) {
    FILE *file = open_capture();
    cli_json_escape(file, "a\"b\\c\nd\te\x01" "f");
    char buffer[128];
    read_capture(file, buffer, sizeof(buffer));
    fclose(file);
    TEST_ASSERT_EQUAL_STRING("a\\\"b\\\\c\\nd\\te\\u0001f", buffer);
}

static void test_json_escape_preserves_valid_utf8(void) {
    FILE *file = open_capture();
    cli_json_escape(file, "中文");
    char buffer[128];
    read_capture(file, buffer, sizeof(buffer));
    fclose(file);
    TEST_ASSERT_EQUAL_STRING("中文", buffer);
}

static void test_json_escape_replaces_invalid_utf8(void) {
    const char invalid[] = {'x', (char)0xFF, 'y', '\0'};
    const char truncated[] = {(char)0xE4, (char)0xB8, '\0'};

    FILE *file = open_capture();
    cli_json_escape(file, invalid);
    char buffer[128];
    read_capture(file, buffer, sizeof(buffer));
    fclose(file);
    TEST_ASSERT_EQUAL_STRING("x\xEF\xBF\xBDy", buffer);

    file = open_capture();
    cli_json_escape(file, truncated);
    read_capture(file, buffer, sizeof(buffer));
    fclose(file);
    TEST_ASSERT_EQUAL_STRING("\xEF\xBF\xBD\xEF\xBF\xBD", buffer);
}

static void test_write_results_json(void) {
    segment_result segments[] = {
        {0.0, 1.5, "hi"},
        {1.5, 2.0, "there"},
    };
    file_result results[] = {
        {"a.wav", segments, 2},
        {NULL, NULL, 0},
    };

    FILE *file = open_capture();
    cli_write_results(file, FORMAT_JSON, results, 2);
    char buffer[512];
    read_capture(file, buffer, sizeof(buffer));
    fclose(file);

    TEST_ASSERT_EQUAL_STRING("[\n"
                             "  {\"file\": \"a.wav\", \"segments\": [\n"
                             "    {\"start\": 0.000, \"end\": 1.500, \"text\": \"hi\"},\n"
                             "    {\"start\": 1.500, \"end\": 2.000, \"text\": \"there\"}\n"
                             "  ]},\n"
                             "  {\"file\": \"\", \"segments\": [\n"
                             "  ]}\n"
                             "]\n",
                             buffer);
}

static void test_write_results_txt(void) {
    segment_result first[] = {{0.0, 1.0, "x"}};
    segment_result second[] = {{1.0, 2.0, "y"}};
    file_result results[] = {
        {"a", first, 1},
        {"b", second, 1},
    };

    FILE *file = open_capture();
    cli_write_results(file, FORMAT_TXT, results, 2);
    char buffer[128];
    read_capture(file, buffer, sizeof(buffer));
    fclose(file);

    TEST_ASSERT_EQUAL_STRING("==> a <==\nx\n\n==> b <==\ny\n", buffer);
}

static void test_write_results_srt(void) {
    segment_result first[] = {{0.0, 1.5, "x"}};
    segment_result second[] = {{2.0, 3.0, "y"}};
    file_result results[] = {
        {"a", first, 1},
        {"b", second, 1},
    };

    FILE *file = open_capture();
    cli_write_results(file, FORMAT_SRT, results, 2);
    char buffer[256];
    read_capture(file, buffer, sizeof(buffer));
    fclose(file);

    TEST_ASSERT_EQUAL_STRING("1\n"
                             "00:00:00,000 --> 00:00:01,500\n"
                             "x\n"
                             "\n"
                             "2\n"
                             "00:00:02,000 --> 00:00:03,000\n"
                             "y\n"
                             "\n",
                             buffer);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_timestamp_formats);
    RUN_TEST(test_json_escape_control_characters);
    RUN_TEST(test_json_escape_preserves_valid_utf8);
    RUN_TEST(test_json_escape_replaces_invalid_utf8);
    RUN_TEST(test_write_results_json);
    RUN_TEST(test_write_results_txt);
    RUN_TEST(test_write_results_srt);
    return UNITY_END();
}
