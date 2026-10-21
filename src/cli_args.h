#ifndef NPUT_CLI_ARGS_H
#define NPUT_CLI_ARGS_H

#include <stddef.h>
#include <stdint.h>

#include "cli_output.h"

/** 输入文件数量上限；与错误信息中的提示保持一致。 */
#define NPUT_CLI_MAX_FILES 256

/** 解析失败时保存的错误文本容量。 */
#define NPUT_CLI_ERROR_CAPACITY 256

typedef struct cli_options {
    const char *model_dir;
    const char *vocab_path;
    const char *encoder_device;
    const char *decoder_device;
    const char *cache_dir;
    const char *language;
    const char *output;
    output_format format;
    int32_t max_tokens;
    int verbose;
} cli_options;

/** 解析结果：要么执行某项立即退出的动作，要么带着选项运行。 */
typedef enum cli_action {
    CLI_ACTION_RUN = 0,
    CLI_ACTION_HELP,
    CLI_ACTION_VERSION,
    CLI_ACTION_LIST_DEVICES,
    CLI_ACTION_LIST_MODELS,
    CLI_ACTION_ERROR
} cli_action;

typedef struct cli_arguments {
    cli_action action;
    cli_options options;
    const char *files[NPUT_CLI_MAX_FILES];
    size_t file_count;
    char error[NPUT_CLI_ERROR_CAPACITY];
} cli_arguments;

/**
 * @brief 解析十进制正整数参数。
 * @param text 待解析文本；必须整体为十进制数字。
 * @param out_value 成功时接收结果。
 * @return 成功返回 1；语法错误、溢出或超出 [1, INT32_MAX] 返回 0。
 */
int cli_parse_int(const char *text, int32_t *out_value);

/**
 * @brief 解析命令行参数，只做语法与取值检查，不做 I/O。
 *
 * 遇到 `-h`、`--version`、`--list-devices`、`--list-models` 时立即停止解析并返回
 * 对应动作，由调用方决定输出什么。失败时 `action` 为 ::CLI_ACTION_ERROR，
 * `error` 中保存可直接打印的错误文本（不含换行）。
 *
 * @param argc argv 长度；`argc >= 1` 时 `argv[0]` 视为程序名。
 * @param argv 参数数组。
 * @param out_arguments 结果；函数内部会先清零并写入默认值。
 */
void cli_parse_arguments(int argc, char **argv, cli_arguments *out_arguments);

#endif
