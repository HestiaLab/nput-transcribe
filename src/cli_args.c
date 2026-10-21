#include "cli_args.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cli_parse_int(const char *text, int32_t *out_value) {
    if (text == NULL || out_value == NULL) {
        return 0;
    }
    char *end = NULL;
    errno = 0;
    const long long value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || value < 1 || value > INT32_MAX) {
        return 0;
    }
    *out_value = (int32_t)value;
    return 1;
}

/* 需要单独传值的选项；offset 指向 cli_options 中对应的字符串字段。 */
typedef struct value_option {
    const char *name;
    const char *alias;
    size_t offset;
} value_option;

static const value_option VALUE_OPTIONS[] = {
    {"--model", "-m", offsetof(cli_options, model_dir)},
    {"--vocab", NULL, offsetof(cli_options, vocab_path)},
    {"--encoder-device", NULL, offsetof(cli_options, encoder_device)},
    {"--decoder-device", NULL, offsetof(cli_options, decoder_device)},
    {"--cache-dir", NULL, offsetof(cli_options, cache_dir)},
    {"--language", "-l", offsetof(cli_options, language)},
    {"--output", "-o", offsetof(cli_options, output)},
};

static int matches(const char *argument, const char *name, const char *alias) {
    return strcmp(argument, name) == 0 || (alias != NULL && strcmp(argument, alias) == 0);
}

static int fail(cli_arguments *out, const char *format, ...) {
    va_list args;
    va_start(args, format);
    (void)vsnprintf(out->error, sizeof(out->error), format, args);
    va_end(args);
    out->action = CLI_ACTION_ERROR;
    return 0;
}

static int push_file(cli_arguments *out, const char *path) {
    if (out->file_count == NPUT_CLI_MAX_FILES) {
        return fail(out, "错误：输入文件过多（最多 %zu 个）", (size_t)NPUT_CLI_MAX_FILES);
    }
    out->files[out->file_count++] = path;
    return 1;
}

/* 返回 0 表示解析失败，1 表示命中并取到值，-1 表示不是带值选项。 */
static int take_value_option(cli_arguments *out, int argc, char **argv, int *index) {
    const char *argument = argv[*index];
    for (size_t i = 0; i < sizeof(VALUE_OPTIONS) / sizeof(VALUE_OPTIONS[0]); ++i) {
        const value_option *option = &VALUE_OPTIONS[i];
        if (!matches(argument, option->name, option->alias)) {
            continue;
        }
        if (*index + 1 >= argc) {
            return fail(out, "错误：%s 需要一个值", argument);
        }
        const char **slot = (const char **)((char *)&out->options + option->offset);
        *slot = argv[++(*index)];
        return 1;
    }
    return -1;
}

void cli_parse_arguments(int argc, char **argv, cli_arguments *out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->action = CLI_ACTION_RUN;
    out->options.language = "auto";
    out->options.format = FORMAT_TXT;

    int options_ended = 0;
    for (int i = 1; i < argc; ++i) {
        const char *argument = argv[i];

        if (!options_ended && strcmp(argument, "--") == 0) {
            options_ended = 1;
            continue;
        }
        if (options_ended) {
            if (!push_file(out, argument)) {
                return;
            }
            continue;
        }

        if (matches(argument, "--help", "-h")) {
            out->action = CLI_ACTION_HELP;
            return;
        }
        if (strcmp(argument, "--version") == 0) {
            out->action = CLI_ACTION_VERSION;
            return;
        }
        if (strcmp(argument, "--list-devices") == 0) {
            out->action = CLI_ACTION_LIST_DEVICES;
            return;
        }
        if (strcmp(argument, "--list-models") == 0) {
            out->action = CLI_ACTION_LIST_MODELS;
            return;
        }
        if (matches(argument, "--verbose", "-v")) {
            out->options.verbose = 1;
            continue;
        }

        /* 取值必须单独传入，--name=value 一律拒绝。 */
        if (strncmp(argument, "--", 2) == 0 && strchr(argument, '=') != NULL) {
            fail(out, "错误：%s 的取值需单独传入（不支持 = 形式）", argument);
            return;
        }

        const int taken = take_value_option(out, argc, argv, &i);
        if (taken == 0) {
            return;
        }
        if (taken == 1) {
            continue;
        }

        if (matches(argument, "--format", "-f")) {
            if (i + 1 >= argc) {
                fail(out, "错误：%s 需要一个值", argument);
                return;
            }
            const char *value = argv[++i];
            if (strcmp(value, "txt") == 0) {
                out->options.format = FORMAT_TXT;
            } else if (strcmp(value, "srt") == 0) {
                out->options.format = FORMAT_SRT;
            } else if (strcmp(value, "json") == 0) {
                out->options.format = FORMAT_JSON;
            } else {
                fail(out, "错误：未知格式 '%s'", value);
                return;
            }
            continue;
        }
        if (strcmp(argument, "--max-tokens") == 0) {
            if (i + 1 >= argc) {
                fail(out, "错误：%s 需要一个值", argument);
                return;
            }
            if (!cli_parse_int(argv[++i], &out->options.max_tokens)) {
                fail(out, "错误：--max-tokens 需要 1 到 %ld 之间的整数", (long)INT32_MAX);
                return;
            }
            continue;
        }
        if (argument[0] == '-' && argument[1] != '\0') {
            fail(out, "错误：未知选项 '%s'", argument);
            return;
        }
        if (!push_file(out, argument)) {
            return;
        }
    }

    if (out->file_count == 0) {
        fail(out, "错误：未指定输入文件");
    }
}

