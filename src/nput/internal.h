#ifndef NPUT_INTERNAL_H
#define NPUT_INTERNAL_H

#include <stdio.h>

#include "nput/audio.h"

/** @brief 将 RIFF/WAVE 文件解码为单声道浮点采样。 */
nput_status nput_wav_decode(const char *path, nput_audio *out);

/**
 * @brief 通过平台解码器解码压缩媒体文件。
 * @return 平台没有对应解码器时返回 NPUT_ERR_UNSUPPORTED。
 */
nput_status nput_platform_decode(const char *path, nput_audio *out);

/** @brief 使用平台文件接口打开 UTF-8 路径。 */
FILE *nput_file_open(const char *utf8_path, const char *mode);

/**
 * @brief 递归创建 `utf8_path` 的父目录。
 * @param utf8_path 目标文件路径；目录已存在时视为成功。
 * @return 成功返回 NPUT_OK；无法创建时返回 NPUT_ERR_IO。
 */
nput_status NPUT_API nput_create_parent_dirs(const char *utf8_path);

/**
 * @brief 自动查找本机的 Whisper IR 目录。
 *
 * 依次检查当前目录的 `./models` 与 Hugging Face 缓存
 * （`HF_HOME/hub` 或 `~/.cache/huggingface/hub` 下的 `models--*whisper*`）。
 * 多个候选取编码器文件最新修改的一个。
 *
 * @param out_path 成功时接收新分配的 UTF-8 路径；用 free 释放。
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

#endif
