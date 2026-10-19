#ifndef NPUT_SEGMENT_H
#define NPUT_SEGMENT_H

#include <stddef.h>

#include "nput/export.h"
#include "nput/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 源缓冲区内的半开采样区间 `[begin, end)`。 */
typedef struct nput_segment {
    size_t begin;
    size_t end;
} nput_segment;

/**
 * @brief 将音频切分成适合单个 Whisper 窗口的片段。
 *
 * 切点落在上限附近最安静的分析帧上，避免从词中间切断；首尾静音会被保留，
 * 以便输出的时间轴与输入对齐。
 *
 * @param samples 单声道采样点；count 不为 0 时不可为 NULL。
 * @param count 采样点数量。
 * @param sample_rate 采样率（Hz），必须大于 0。
 * @param max_samples 单个片段的最大长度（采样点），必须大于 0。
 * @param out_segments 接收新分配的数组；用 ::nput_segments_free 释放。
 * @param out_count 接收片段数量。
 * @return 成功时返回 NPUT_OK。
 */
nput_status NPUT_API nput_segment_split(const float *samples, size_t count, int sample_rate,
                                        size_t max_samples, nput_segment **out_segments,
                                        size_t *out_count);

/**
 * @brief 释放片段数组。
 * @param segments ::nput_segment_split 返回的数组；允许为 NULL。
 */
void NPUT_API nput_segments_free(nput_segment *segments);

#ifdef __cplusplus
}
#endif

#endif
