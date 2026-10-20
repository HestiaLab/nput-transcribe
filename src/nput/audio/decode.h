#ifndef NPUT_DECODE_H
#define NPUT_DECODE_H

#include "nput/audio.h"

/** @brief 将 RIFF/WAVE 文件解码为单声道浮点采样。 */
nput_status nput_wav_decode(const char *path, nput_audio *out);

/**
 * @brief 通过平台解码器解码压缩媒体文件。
 * @return 平台没有对应解码器时返回 NPUT_ERR_UNSUPPORTED。
 */
nput_status nput_platform_decode(const char *path, nput_audio *out);

#endif
