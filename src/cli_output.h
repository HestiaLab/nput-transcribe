#ifndef NPUT_CLI_OUTPUT_H
#define NPUT_CLI_OUTPUT_H

#include <stddef.h>
#include <stdio.h>

typedef enum output_format { FORMAT_TXT = 0, FORMAT_SRT, FORMAT_JSON } output_format;

typedef struct segment_result {
    double start;
    double end;
    char *text;
} segment_result;

typedef struct file_result {
    const char *path;
    segment_result *segments;
    size_t count;
} file_result;

/** @brief 把秒数写成 SRT 时间戳；负数按 0 处理。 */
void cli_format_timestamp(char *buffer, size_t size, double seconds);

/** @brief 按 JSON 字符串转义写入；非法 UTF-8 字节替换为 U+FFFD。 */
void cli_json_escape(FILE *stream, const char *text);

/** @brief 按指定格式写出全部结果；`path` 为 NULL 时按空字符串处理。 */
void cli_write_results(FILE *stream, output_format format, const file_result *results,
                       size_t result_count);

#endif
