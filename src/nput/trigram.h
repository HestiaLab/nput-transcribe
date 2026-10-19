#ifndef NPUT_TRIGRAM_H
#define NPUT_TRIGRAM_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief 当追加 `candidate` 会重复 `ids` 中已出现的 3-gram 时返回非零。
 *
 * Whisper 的长音频解码使用同一规则（`no_repeat_ngram_size=3`），用于跳出
 * 音乐、噪声和静音上的重复循环。
 *
 * @param ids 已生成的 token id，按时间从旧到新；count >= 2 时不可为 NULL。
 * @param count 已生成的 token 数量。
 * @param candidate 贪心解码提出的候选 token。
 * @return 当 3-gram (ids[count-2], ids[count-1], candidate) 已出现时返回 1。
 */
static inline int nput_trigram_repeats(const int64_t *ids, size_t count, int32_t candidate) {
    if (count < 2) {
        return 0;
    }
    const int64_t first = ids[count - 2];
    const int64_t second = ids[count - 1];
    for (size_t i = 0; i + 2 < count; ++i) {
        if (ids[i] == first && ids[i + 1] == second && ids[i + 2] == (int64_t)candidate) {
            return 1;
        }
    }
    return 0;
}

#endif
