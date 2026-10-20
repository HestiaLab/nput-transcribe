#ifndef NPUT_PATH_H
#define NPUT_PATH_H

#include <stdio.h>

#include "nput/export.h"
#include "nput/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 使用平台文件接口打开 UTF-8 路径。
 *
 * Windows 下通过宽字符 API 打开，避免 CRT 按 ANSI 代码页解释路径。
 *
 * @param utf8_path UTF-8 编码的路径；不可为 NULL。
 * @param mode 与 `fopen` 相同的模式字符串；不可为 NULL。
 * @return 成功返回文件流；失败返回 NULL。
 */
FILE *NPUT_API nput_file_open(const char *utf8_path, const char *mode);

/**
 * @brief 递归创建 `utf8_path` 的父目录。
 * @param utf8_path 目标文件路径；目录已存在时视为成功。
 * @return 成功返回 NPUT_OK；无法创建时返回 NPUT_ERR_IO。
 */
nput_status NPUT_API nput_create_parent_dirs(const char *utf8_path);

#ifdef __cplusplus
}
#endif

#endif
