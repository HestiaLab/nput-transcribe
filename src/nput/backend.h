#ifndef NPUT_BACKEND_H
#define NPUT_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include "nput/asr.h"
#include "nput/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 模型后端需要实现的内部接口。
 *
 * `create` 只在调用期间看到 `options`，不得保存其中的字符串指针。
 * `transcribe` 负责分配文本；成功后通过 `out_timings` 返回本次耗时。
 * `detect_language` 不更新 ::nput_asr_last_timings。
 */
typedef struct nput_asr_backend {
    const char *name; /**< 选择用名称；区分大小写。 */
    nput_status (*create)(const nput_asr_options *options, void **out_state);
    nput_status (*detect_language)(void *state, const float *samples, size_t count,
                                   int32_t *out_token);
    nput_status (*transcribe)(void *state, const float *samples, size_t count,
                              int32_t language_token, char **out_text,
                              nput_asr_timings *out_timings);
    void (*destroy)(void *state);
} nput_asr_backend;

/** @brief 默认后端（Whisper）。 */
extern const nput_asr_backend nput_asr_backend_whisper;

/**
 * @brief 按名称查找后端。
 * @param name 后端名称；NULL 或空字符串返回默认后端。
 * @return 找到返回后端表；未知名称返回 NULL。
 */
const nput_asr_backend *nput_asr_backend_find(const char *name);

/**
 * @brief 注册或替换后端。
 *
 * 仅供测试与启动阶段使用，不线程安全。同名后端会被替换。
 *
 * @param backend 要注册的后端；函数指针必须完整。
 * @return 成功时返回 NPUT_OK；参数不完整时返回 NPUT_ERR_INVALID_ARGUMENT；
 *         注册表已满时返回 NPUT_ERR_OUT_OF_MEMORY。
 */
nput_status NPUT_API nput_asr_backend_register(const nput_asr_backend *backend);

#ifdef __cplusplus
}
#endif

#endif
