#ifndef NPUT_CLI_ARGS_H
#define NPUT_CLI_ARGS_H

#include <stdint.h>

/**
 * @brief 解析十进制正整数参数。
 * @param text 待解析文本；必须整体为十进制数字。
 * @param out_value 成功时接收结果。
 * @return 成功返回 1；语法错误、溢出或超出 [1, INT32_MAX] 返回 0。
 */
int cli_parse_int(const char *text, int32_t *out_value);

#endif
