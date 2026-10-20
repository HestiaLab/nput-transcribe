#ifndef NPUT_MODEL_H
#define NPUT_MODEL_H

#include "nput/export.h"
#include "nput/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 自动查找本机的 Whisper IR 目录。
 *
 * 依次检查当前目录的 `./models` 与 Hugging Face 缓存
 * （`HF_HOME/hub` 或 `~/.cache/huggingface/hub` 下的 `models--*whisper*`）。
 * 多个候选取编码器文件最新修改的一个。
 *
 * @param out_path 成功时接收新分配的 UTF-8 路径；用 ::nput_free 释放。
 * @return 找到返回 NPUT_OK；没有候选返回 NPUT_ERR_UNSUPPORTED。
 */
nput_status NPUT_API nput_find_default_model_dir(char **out_path);

/**
 * @brief 列出自动查找可用的所有模型目录。
 *
 * 与 ::nput_find_default_model_dir 的查找范围相同，按编码器文件修改时间从新到旧排列。
 *
 * @param out_lines 成功时接收以换行分隔的 UTF-8 路径；用 ::nput_free 释放。
 * @return 找到返回 NPUT_OK；没有候选返回 NPUT_ERR_UNSUPPORTED。
 */
nput_status NPUT_API nput_list_model_dirs(char **out_lines);

#ifdef __cplusplus
}
#endif

#endif
