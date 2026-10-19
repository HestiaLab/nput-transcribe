#ifndef NPUT_ASR_H
#define NPUT_ASR_H

#include <stddef.h>
#include <stdint.h>

#include "nput/export.h"
#include "nput/mel.h"
#include "nput/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 持有已编译编码器与解码器的 Whisper 会话。 */
typedef struct nput_asr nput_asr;

/** @brief ::nput_asr_create 的构造选项。 */
typedef struct nput_asr_options {
    const char *model_dir;      /**< 存放 OpenVINO Whisper IR 文件的目录。 */
    const char *vocab_path;     /**< 可选的 `vocab.json` 路径；默认取 model_dir。 */
    const char *encoder_device; /**< 编码器使用的 OpenVINO 设备；默认 "NPU"。 */
    const char *decoder_device; /**< 解码器使用的 OpenVINO 设备；默认 "CPU"。 */
    const char *cache_dir;      /**< 可选的编译缓存根目录。 */
    int32_t max_tokens;         /**< 每段解码上限；<= 0 时取 224。 */
    int verbose;                /**< 非零时在 stderr 输出进度。 */
} nput_asr_options;

/** @brief 最近一次 ::nput_asr_transcribe 调用的耗时。 */
typedef struct nput_asr_timings {
    double encoder_ms; /**< 编码器墙钟耗时。 */
    double decoder_ms; /**< 解码器墙钟耗时。 */
    size_t tokens;     /**< 生成的 token 数。 */
} nput_asr_timings;

/**
 * @brief 用默认值填充选项结构。
 * @param options 要初始化的结构；不可为 NULL。
 */
void NPUT_API nput_asr_options_init(nput_asr_options *options);

/**
 * @brief 创建会话，并在目标设备上编译两个模型。
 * @param options 已校验的选项；`model_dir` 必须设置。
 * @param out_asr 接收新分配的会话。
 * @return 成功时返回 NPUT_OK；OpenVINO 拒绝模型时返回 NPUT_ERR_RUNTIME。
 */
nput_status NPUT_API nput_asr_create(const nput_asr_options *options, nput_asr **out_asr);

/**
 * @brief 销毁会话。
 * @param asr 要销毁的会话；允许为 NULL。
 */
void NPUT_API nput_asr_destroy(nput_asr *asr);

/**
 * @brief 列出本机可用的 OpenVINO 设备。
 * @param out_csv 接收逗号分隔的设备列表；用 ::nput_free 释放。
 * @return 成功时返回 NPUT_OK。
 */
nput_status NPUT_API nput_asr_devices(char **out_csv);

/**
 * @brief 将语言代码映射为 Whisper 的提示 token。
 * @param code 两个字母的代码，如 "en"、"zh"；NULL 或 "auto" 表示自动检测。
 * @return token id；NULL、"auto" 或未知代码返回 -1（其中 "auto" 表示运行时检测）。
 */
int32_t NPUT_API nput_asr_language_token(const char *code);

/**
 * @brief 从编码器输出中检测语言。
 * @param asr 要使用的会话；不可为 NULL。
 * @param mel 该片段的频谱。
 * @param out_token 接收语言 token id。
 * @return 成功时返回 NPUT_OK。
 */
nput_status NPUT_API nput_asr_detect_language(nput_asr *asr, const nput_mel *mel,
                                              int32_t *out_token);

/**
 * @brief 转写一个 30 秒片段。
 * @param asr 要使用的会话；不可为 NULL。
 * @param mel 该片段的频谱。
 * @param language_token 语言 token；-1 表示自动检测。
 * @param out_text 接收转写文本；用 ::nput_free 释放。
 * @return 成功时返回 NPUT_OK。
 */
nput_status NPUT_API nput_asr_transcribe(nput_asr *asr, const nput_mel *mel,
                                         int32_t language_token, char **out_text);

/**
 * @brief 返回最近一次转写的耗时。
 * @param asr 要查询的会话；不可为 NULL。
 * @param out_timings 接收耗时数据。
 */
void NPUT_API nput_asr_last_timings(const nput_asr *asr, nput_asr_timings *out_timings);

#ifdef __cplusplus
}
#endif

#endif
