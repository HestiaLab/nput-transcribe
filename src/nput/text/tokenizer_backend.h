#ifndef NPUT_TOKENIZER_BACKEND_H
#define NPUT_TOKENIZER_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include "nput/status.h"
#include "nput/tokenizer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* Whisper 字节级 BPE 词表上界；也是 eot 与特殊/时间戳 token 的起点。 */
    NPUT_TOKENIZER_SPECIAL_FIRST = 50257
};

/**
 * @brief 解码 token id，并跳过大于等于 `limit` 的 id。
 *
 * 供模型后端指定自己的特殊 token 边界；公共 ::nput_tokenizer_decode 行为不变。
 *
 * @param limit 开始跳过的 id 下界。
 */
nput_status nput_tokenizer_decode_below(const nput_tokenizer *tokenizer, const int64_t *ids,
                                        size_t count, int64_t limit, char **out_text);

#ifdef __cplusplus
}
#endif

#endif
