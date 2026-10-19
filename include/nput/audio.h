#ifndef NPUT_AUDIO_H
#define NPUT_AUDIO_H

#include <stddef.h>

#include "nput/export.h"
#include "nput/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief [-1, 1] 范围内的单声道音频；成功后所有权归调用方。 */
typedef struct nput_audio {
    float *samples;   /**< 单声道采样点。 */
    size_t count;     /**< 采样点数量。 */
    int sample_rate;  /**< 采样率（Hz）。 */
} nput_audio;

/**
 * @brief 将音频缓冲区初始化为空状态。
 * @param audio 要初始化的缓冲区；不可为 NULL。
 */
void NPUT_API nput_audio_init(nput_audio *audio);

/**
 * @brief 释放音频缓冲区持有的采样点并重置。
 * @param audio 要释放的缓冲区；允许为 NULL。
 */
void NPUT_API nput_audio_free(nput_audio *audio);

/**
 * @brief 加载 WAV 文件（8/16/24/32 位整数 PCM 或 32/64 位 IEEE 浮点）。
 * @param path UTF-8 路径。
 * @param out 接收按文件原始采样率解码的单声道音频。
 * @return 成功时返回 NPUT_OK。
 */
nput_status NPUT_API nput_audio_load_wav(const char *path, nput_audio *out);

/**
 * @brief 加载平台解码器支持的任何音频文件。
 *
 * WAV 直接解析；其它容器（Windows 上的 MP3、M4A、FLAC、WMA）交给系统解码器。
 *
 * @param path UTF-8 路径。
 * @param out 接收按文件原始采样率解码的单声道音频。
 * @return 成功时返回 NPUT_OK；没有可用解码器时返回 NPUT_ERR_UNSUPPORTED。
 */
nput_status NPUT_API nput_audio_load(const char *path, nput_audio *out);

/**
 * @brief 使用窗函数 sinc 插值对单声道音频重采样。
 * @param source 输入音频；所有权仍归调用方。
 * @param target_rate 目标采样率（Hz），必须大于 0。
 * @param out 成功时接收新分配的缓冲区。
 * @return 成功时返回 NPUT_OK。
 */
nput_status NPUT_API nput_audio_resample(const nput_audio *source, int target_rate, nput_audio *out);

#ifdef __cplusplus
}
#endif

#endif
