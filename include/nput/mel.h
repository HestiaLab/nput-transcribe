#ifndef NPUT_MEL_H
#define NPUT_MEL_H

#include <stddef.h>

#include "nput/export.h"
#include "nput/status.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NPUT_MEL_RATE = 16000,      /**< Whisper 输入采样率（Hz）。 */
    NPUT_MEL_BINS = 80,         /**< mel 滤波器数量。 */
    NPUT_MEL_WINDOW = 400,      /**< STFT 窗长（采样点）。 */
    NPUT_MEL_HOP = 160,         /**< STFT 帧移（采样点）。 */
    NPUT_MEL_FRAMES = 3000,     /**< 30 秒片段内的帧数。 */
    NPUT_MEL_SAMPLES = 480000   /**< 30 秒片段内的采样点数。 */
};

/** @brief 按 [bins][frames] 行优先排列的 log-mel 频谱。 */
typedef struct nput_mel {
    float *data;    /**< bins * frames 个值，由本结构持有。 */
    size_t bins;    /**< mel 滤波器数量。 */
    size_t frames;  /**< 帧数。 */
} nput_mel;

/**
 * @brief 计算 Whisper 期望的 log-mel 频谱。
 *
 * 输入会被裁剪或补零到恰好 30 秒；频谱使用周期 Hann 窗与半个窗长的反射填充，
 * 然后按 `(max(log10(mel), max - 8) + 4) / 4` 归一化。
 *
 * @param samples ::NPUT_MEL_RATE 采样率的单声道采样点；不可为 NULL。
 * @param count 采样点数量；超出 ::NPUT_MEL_SAMPLES 的部分会被忽略。
 * @param out 成功时接收频谱。
 * @return 成功时返回 NPUT_OK；samples 为 NULL 时返回 NPUT_ERR_INVALID_ARGUMENT。
 */
nput_status NPUT_API nput_mel_compute(const float *samples, size_t count, nput_mel *out);

/**
 * @brief 释放频谱缓冲区。
 * @param mel 要释放的频谱；允许为 NULL。
 */
void NPUT_API nput_mel_free(nput_mel *mel);

#ifdef __cplusplus
}
#endif

#endif
