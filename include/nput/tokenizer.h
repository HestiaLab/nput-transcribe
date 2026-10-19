#ifndef NPUT_TOKENIZER_H
#define NPUT_TOKENIZER_H

#include <stddef.h>
#include <stdint.h>

#include "nput/export.h"
#include "nput/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 用于把 token id 还原为文本的字节级 BPE 词表。 */
typedef struct nput_tokenizer nput_tokenizer;

/**
 * @brief 加载 GPT-2 风格的 `vocab.json`（token 到 id 的扁平 JSON 对象）。
 * @param vocab_json_path `vocab.json` 的 UTF-8 路径。
 * @param out_tokenizer 成功时接收新分配的分词器。
 * @return 成功时返回 NPUT_OK；词表格式错误时返回 NPUT_ERR_FORMAT。
 */
nput_status NPUT_API nput_tokenizer_load(const char *vocab_json_path,
                                         nput_tokenizer **out_tokenizer);

/**
 * @brief 释放分词器。
 * @param tokenizer 要释放的分词器；允许为 NULL。
 */
void NPUT_API nput_tokenizer_free(nput_tokenizer *tokenizer);

/**
 * @brief 将 token id 解码为新分配的 UTF-8 字符串。
 *
 * 大于等于 50257 的 id 是 Whisper 的特殊 token / 时间戳 token，会被跳过。
 *
 * @param tokenizer 要使用的分词器；不可为 NULL。
 * @param ids 要解码的 token id；count 为 0 时允许为 NULL。
 * @param count token id 数量。
 * @param out_text 接收以 NUL 结尾的字符串；用 ::nput_free 释放。
 * @return 成功时返回 NPUT_OK。
 */
nput_status NPUT_API nput_tokenizer_decode(const nput_tokenizer *tokenizer, const int64_t *ids,
                                           size_t count, char **out_text);

/**
 * @brief 返回词表中的字节级 token 数量。
 * @param tokenizer 要查询的分词器；不可为 NULL。
 * @return 词表大小。
 */
size_t NPUT_API nput_tokenizer_vocabulary_size(const nput_tokenizer *tokenizer);

#ifdef __cplusplus
}
#endif

#endif
