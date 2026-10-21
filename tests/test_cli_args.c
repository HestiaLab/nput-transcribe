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

static cli_arguments parse(int argc, char **argv) {
    cli_arguments arguments;
    cli_parse_arguments(argc, argv, &arguments);
    return arguments;
}

static void test_defaults_and_single_file(void) {
    char *argv[] = {"prog", "a.wav"};
    const cli_arguments arguments = parse(2, argv);

    TEST_ASSERT_EQUAL_INT(CLI_ACTION_RUN, arguments.action);
    TEST_ASSERT_EQUAL_INT(FORMAT_TXT, arguments.options.format);
    TEST_ASSERT_EQUAL_STRING("auto", arguments.options.language);
    TEST_ASSERT_EQUAL_INT(0, arguments.options.verbose);
    TEST_ASSERT_EQUAL_INT(0, arguments.options.max_tokens);
    TEST_ASSERT_EQUAL_INT(1, (int)arguments.file_count);
    TEST_ASSERT_EQUAL_STRING("a.wav", arguments.files[0]);
}

static void test_value_options_and_aliases(void) {
    char *argv[] = {"prog",         "-m",           "model-dir", "--vocab", "v.json",
                    "--encoder-device", "GPU",      "--decoder-device", "CPU",
                    "--cache-dir",  "cache",        "-l",        "zh",
                    "-o",           "out.srt",      "-v",        "a.wav"};
    const cli_arguments arguments = parse(17, argv);

    TEST_ASSERT_EQUAL_INT(CLI_ACTION_RUN, arguments.action);
    TEST_ASSERT_EQUAL_STRING("model-dir", arguments.options.model_dir);
    TEST_ASSERT_EQUAL_STRING("v.json", arguments.options.vocab_path);
    TEST_ASSERT_EQUAL_STRING("GPU", arguments.options.encoder_device);
    TEST_ASSERT_EQUAL_STRING("CPU", arguments.options.decoder_device);
    TEST_ASSERT_EQUAL_STRING("cache", arguments.options.cache_dir);
    TEST_ASSERT_EQUAL_STRING("zh", arguments.options.language);
    TEST_ASSERT_EQUAL_STRING("out.srt", arguments.options.output);
    TEST_ASSERT_EQUAL_INT(1, arguments.options.verbose);
    TEST_ASSERT_EQUAL_INT(1, (int)arguments.file_count);
}

static void test_long_aliases(void) {
    char *argv[] = {"prog", "--model", "m", "--language", "en", "--output", "o.txt",
                    "--verbose", "a.wav"};
    const cli_arguments arguments = parse(9, argv);

    TEST_ASSERT_EQUAL_STRING("m", arguments.options.model_dir);
    TEST_ASSERT_EQUAL_STRING("en", arguments.options.language);
    TEST_ASSERT_EQUAL_STRING("o.txt", arguments.options.output);
    TEST_ASSERT_EQUAL_INT(1, arguments.options.verbose);
}

static void test_format_values(void) {
    char *srt[] = {"prog", "-f", "srt", "a.wav"};
    char *json[] = {"prog", "--format", "json", "a.wav"};
    char *bad[] = {"prog", "-f", "yaml", "a.wav"};

    TEST_ASSERT_EQUAL_INT(FORMAT_SRT, parse(4, srt).options.format);
    TEST_ASSERT_EQUAL_INT(FORMAT_JSON, parse(4, json).options.format);

    const cli_arguments failed = parse(4, bad);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_ERROR, failed.action);
    TEST_ASSERT_EQUAL_STRING("错误：未知格式 'yaml'", failed.error);
}

static void test_missing_value(void) {
    char *model[] = {"prog", "-m"};
    char *format[] = {"prog", "--format"};
    char *tokens[] = {"prog", "--max-tokens"};

    const cli_arguments model_failed = parse(2, model);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_ERROR, model_failed.action);
    TEST_ASSERT_EQUAL_STRING("错误：-m 需要一个值", model_failed.error);

    TEST_ASSERT_EQUAL_STRING("错误：--format 需要一个值", parse(2, format).error);
    TEST_ASSERT_EQUAL_STRING("错误：--max-tokens 需要一个值", parse(2, tokens).error);
}

static void test_rejects_equals_form(void) {
    char *argv[] = {"prog", "--model=dir", "a.wav"};
    const cli_arguments arguments = parse(3, argv);

    TEST_ASSERT_EQUAL_INT(CLI_ACTION_ERROR, arguments.action);
    TEST_ASSERT_EQUAL_STRING("错误：--model=dir 的取值需单独传入（不支持 = 形式）",
                             arguments.error);
}

static void test_double_dash_ends_options(void) {
    char *argv[] = {"prog", "--", "-m", "--help"};
    const cli_arguments arguments = parse(4, argv);

    TEST_ASSERT_EQUAL_INT(CLI_ACTION_RUN, arguments.action);
    TEST_ASSERT_EQUAL_INT(2, (int)arguments.file_count);
    TEST_ASSERT_EQUAL_STRING("-m", arguments.files[0]);
    TEST_ASSERT_EQUAL_STRING("--help", arguments.files[1]);
}

static void test_dash_alone_is_a_file(void) {
    char *argv[] = {"prog", "-"};
    const cli_arguments arguments = parse(2, argv);

    TEST_ASSERT_EQUAL_INT(CLI_ACTION_RUN, arguments.action);
    TEST_ASSERT_EQUAL_INT(1, (int)arguments.file_count);
    TEST_ASSERT_EQUAL_STRING("-", arguments.files[0]);
}

static void test_unknown_options(void) {
    char *long_option[] = {"prog", "--bogus", "a.wav"};
    char *short_option[] = {"prog", "-x", "a.wav"};

    const cli_arguments long_failed = parse(3, long_option);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_ERROR, long_failed.action);
    TEST_ASSERT_EQUAL_STRING("错误：未知选项 '--bogus'", long_failed.error);

    const cli_arguments short_failed = parse(3, short_option);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_ERROR, short_failed.action);
    TEST_ASSERT_EQUAL_STRING("错误：未知选项 '-x'", short_failed.error);
}

static void test_actions_stop_parsing(void) {
    char *help[] = {"prog", "--help", "--bogus"};
    char *short_help[] = {"prog", "-h", "--bogus"};
    char *version[] = {"prog", "--version", "--bogus"};
    char *devices[] = {"prog", "--list-devices", "--bogus"};
    char *models[] = {"prog", "--list-models", "--bogus"};

    TEST_ASSERT_EQUAL_INT(CLI_ACTION_HELP, parse(3, help).action);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_HELP, parse(3, short_help).action);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_VERSION, parse(3, version).action);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_LIST_DEVICES, parse(3, devices).action);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_LIST_MODELS, parse(3, models).action);
}

static void test_requires_input_file(void) {
    char *argv[] = {"prog", "-v"};
    const cli_arguments arguments = parse(2, argv);

    TEST_ASSERT_EQUAL_INT(CLI_ACTION_ERROR, arguments.action);
    TEST_ASSERT_EQUAL_STRING("错误：未指定输入文件", arguments.error);
}

static void test_max_tokens_bounds(void) {
    char *ok[] = {"prog", "--max-tokens", "512", "a.wav"};
    char *zero[] = {"prog", "--max-tokens", "0", "a.wav"};

    TEST_ASSERT_EQUAL_INT(512, parse(4, ok).options.max_tokens);

    const cli_arguments failed = parse(4, zero);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_ERROR, failed.action);
    TEST_ASSERT_EQUAL_STRING("错误：--max-tokens 需要 1 到 2147483647 之间的整数", failed.error);
}

static void test_rejects_too_many_files(void) {
    static char *argv[NPUT_CLI_MAX_FILES + 2];
    argv[0] = "prog";
    for (size_t i = 1; i <= NPUT_CLI_MAX_FILES + 1; ++i) {
        argv[i] = "a.wav";
    }

    const cli_arguments arguments = parse((int)(NPUT_CLI_MAX_FILES + 2), argv);
    TEST_ASSERT_EQUAL_INT(CLI_ACTION_ERROR, arguments.action);
    TEST_ASSERT_EQUAL_STRING("错误：输入文件过多（最多 256 个）", arguments.error);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_accepts_valid_values);
    RUN_TEST(test_rejects_out_of_range);
    RUN_TEST(test_rejects_malformed);
    RUN_TEST(test_rejects_null_arguments);
    RUN_TEST(test_defaults_and_single_file);
    RUN_TEST(test_value_options_and_aliases);
    RUN_TEST(test_long_aliases);
    RUN_TEST(test_format_values);
    RUN_TEST(test_missing_value);
    RUN_TEST(test_rejects_equals_form);
    RUN_TEST(test_double_dash_ends_options);
    RUN_TEST(test_dash_alone_is_a_file);
    RUN_TEST(test_unknown_options);
    RUN_TEST(test_actions_stop_parsing);
    RUN_TEST(test_requires_input_file);
    RUN_TEST(test_max_tokens_bounds);
    RUN_TEST(test_rejects_too_many_files);
    return UNITY_END();
}
