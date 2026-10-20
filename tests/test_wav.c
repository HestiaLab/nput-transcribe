#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nput/audio.h"
#include "nput/status.h"
#include "unity.h"

static void put_u16(unsigned char *target, uint16_t value) {
    target[0] = (unsigned char)(value & 0xFF);
    target[1] = (unsigned char)(value >> 8);
}

static void put_u32(unsigned char *target, uint32_t value) {
    target[0] = (unsigned char)(value & 0xFF);
    target[1] = (unsigned char)((value >> 8) & 0xFF);
    target[2] = (unsigned char)((value >> 16) & 0xFF);
    target[3] = (unsigned char)((value >> 24) & 0xFF);
}

/* 写入一个最小 RIFF/WAVE 文件，返回其大小。 */
static size_t build_wav(unsigned char *buffer, uint16_t channels, uint16_t bits, uint32_t rate,
                        const unsigned char *data, uint32_t data_size, uint16_t tag) {
    const uint16_t block_align = (uint16_t)(channels * bits / 8u);
    memcpy(buffer, "RIFF", 4);
    put_u32(buffer + 4, 36 + data_size);
    memcpy(buffer + 8, "WAVE", 4);
    memcpy(buffer + 12, "fmt ", 4);
    put_u32(buffer + 16, 16);
    put_u16(buffer + 20, tag);
    put_u16(buffer + 22, channels);
    put_u32(buffer + 24, rate);
    put_u32(buffer + 28, rate * block_align);
    put_u16(buffer + 32, block_align);
    put_u16(buffer + 34, bits);
    memcpy(buffer + 36, "data", 4);
    put_u32(buffer + 40, data_size);
    memcpy(buffer + 44, data, data_size);
    return 44 + data_size;
}

static nput_audio load_from(unsigned char *bytes, size_t size) {
    const char *path = "nput_test_tmp.wav";
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(size, fwrite(bytes, 1, size, file));
    fclose(file);

    nput_audio audio;
    nput_status status = nput_audio_load_wav(path, &audio);
    remove(path);
    TEST_ASSERT_EQUAL(NPUT_OK, status);
    return audio;
}

void setUp(void) {}
void tearDown(void) {}

static void test_wav_pcm16_mono(void) {
    unsigned char data[8];
    put_u16(data + 0, 0);
    put_u16(data + 2, 16384);
    put_u16(data + 4, (uint16_t)0xC000); /* -16384 */
    put_u16(data + 6, 32767);

    unsigned char wav[128];
    const size_t size = build_wav(wav, 1, 16, 8000, data, sizeof(data), 1);
    nput_audio audio = load_from(wav, size);

    TEST_ASSERT_EQUAL_INT(8000, audio.sample_rate);
    TEST_ASSERT_EQUAL_size_t(4, audio.count);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, audio.samples[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, audio.samples[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.5f, audio.samples[2]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.99997f, audio.samples[3]);
    nput_audio_free(&audio);
}

static void test_wav_pcm16_stereo_is_averaged(void) {
    unsigned char data[8];
    put_u16(data + 0, 16384);
    put_u16(data + 2, (uint16_t)0xC000);
    put_u16(data + 4, 32767);
    put_u16(data + 6, 32767);

    unsigned char wav[128];
    const size_t size = build_wav(wav, 2, 16, 44100, data, sizeof(data), 1);
    nput_audio audio = load_from(wav, size);

    TEST_ASSERT_EQUAL_size_t(2, audio.count);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, audio.samples[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.99997f, audio.samples[1]);
    nput_audio_free(&audio);
}

static void test_wav_pcm24_mono(void) {
    unsigned char data[3] = {0x00, 0x00, 0x40}; /* 0x400000 表示半量程 */
    unsigned char wav[128];
    const size_t size = build_wav(wav, 1, 24, 16000, data, sizeof(data), 1);
    nput_audio audio = load_from(wav, size);

    TEST_ASSERT_EQUAL_size_t(1, audio.count);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, audio.samples[0]);
    nput_audio_free(&audio);
}

static void test_wav_float32_mono(void) {
    unsigned char data[4];
    const float value = 0.25f;
    memcpy(data, &value, sizeof(value));
    unsigned char wav[128];
    const size_t size = build_wav(wav, 1, 32, 16000, data, sizeof(data), 3);
    nput_audio audio = load_from(wav, size);

    TEST_ASSERT_EQUAL_size_t(1, audio.count);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, audio.samples[0]);
    nput_audio_free(&audio);
}

static void test_wav_rejects_garbage(void) {
    unsigned char junk[64];
    memset(junk, 0x7F, sizeof(junk));
    const char *path = "nput_test_tmp.wav";
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    fwrite(junk, 1, sizeof(junk), file);
    fclose(file);
    nput_audio audio;
    const nput_status status = nput_audio_load_wav(path, &audio);
    remove(path);
    TEST_ASSERT_EQUAL(NPUT_ERR_FORMAT, status);
}

static nput_status load_bytes(const unsigned char *bytes, size_t size, nput_audio *audio) {
    const char *path = "nput_test_tmp.wav";
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(size, fwrite(bytes, 1, size, file));
    fclose(file);
    const nput_status status = nput_audio_load_wav(path, audio);
    remove(path);
    return status;
}

static void test_wav_rejects_short_fmt(void) {
    unsigned char wav[64];
    size_t offset = 0;
    memcpy(wav + offset, "RIFF", 4);
    put_u32(wav + offset + 4, 38);
    memcpy(wav + offset + 8, "WAVE", 4);
    offset += 12;
    memcpy(wav + offset, "fmt ", 4);
    put_u32(wav + offset + 4, 14); /* 小于固定字段所需的 16 字节 */
    offset += 8;
    memset(wav + offset, 0, 14);
    put_u16(wav + offset, 1);
    put_u16(wav + offset + 2, 1);
    put_u32(wav + offset + 4, 16000);
    put_u32(wav + offset + 8, 32000);
    put_u16(wav + offset + 12, 2);
    offset += 14;
    memcpy(wav + offset, "data", 4);
    put_u32(wav + offset + 4, 2);
    offset += 8;
    wav[offset++] = 0;
    wav[offset++] = 0;

    nput_audio audio;
    TEST_ASSERT_EQUAL(NPUT_ERR_FORMAT, load_bytes(wav, offset, &audio));
}

static void test_wav_skips_odd_chunk_padding(void) {
    unsigned char data[4] = {0, 0, 0, 0};
    unsigned char wav[128];
    memcpy(wav, "RIFF", 4);
    put_u32(wav + 4, 12 + 24 + 12 + sizeof(data));
    memcpy(wav + 8, "WAVE", 4);
    memcpy(wav + 12, "fmt ", 4);
    put_u32(wav + 16, 16);
    put_u16(wav + 20, 1);
    put_u16(wav + 22, 1);
    put_u32(wav + 24, 16000);
    put_u32(wav + 28, 32000);
    put_u16(wav + 32, 2);
    put_u16(wav + 34, 16);
    memcpy(wav + 36, "LIST", 4);
    put_u32(wav + 40, 3); /* 奇数长度块，后面必须跳过 1 字节填充 */
    wav[44] = 'a';
    wav[45] = 'b';
    wav[46] = 'c';
    wav[47] = 0;
    memcpy(wav + 48, "data", 4);
    put_u32(wav + 52, sizeof(data));
    memcpy(wav + 56, data, sizeof(data));

    nput_audio audio;
    TEST_ASSERT_EQUAL(NPUT_OK, load_bytes(wav, 56 + sizeof(data), &audio));
    TEST_ASSERT_EQUAL_size_t(2, audio.count);
    nput_audio_free(&audio);
}

static void test_wav_rejects_pcm64(void) {
    unsigned char data[8] = {0};
    unsigned char wav[128];
    const size_t size = build_wav(wav, 1, 64, 16000, data, sizeof(data), 1);

    nput_audio audio;
    TEST_ASSERT_EQUAL(NPUT_ERR_FORMAT, load_bytes(wav, size, &audio));
}

static void test_wav_rejects_truncated_data(void) {
    unsigned char data[8] = {0};
    unsigned char wav[128];
    const size_t size = build_wav(wav, 1, 16, 16000, data, sizeof(data), 1);

    const char *path = "nput_test_tmp.wav";
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    /* data 块声明 8 字节，实际文件只有 4 字节。 */
    TEST_ASSERT_EQUAL_size_t(size - 4, fwrite(wav, 1, size - 4, file));
    fclose(file);

    nput_audio audio;
    const nput_status status = nput_audio_load_wav(path, &audio);
    remove(path);
    TEST_ASSERT_EQUAL(NPUT_ERR_IO, status);
    TEST_ASSERT_NULL(audio.samples);
    TEST_ASSERT_EQUAL_size_t(0, audio.count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wav_pcm16_mono);
    RUN_TEST(test_wav_pcm16_stereo_is_averaged);
    RUN_TEST(test_wav_pcm24_mono);
    RUN_TEST(test_wav_float32_mono);
    RUN_TEST(test_wav_rejects_garbage);
    RUN_TEST(test_wav_rejects_short_fmt);
    RUN_TEST(test_wav_skips_odd_chunk_padding);
    RUN_TEST(test_wav_rejects_pcm64);
    RUN_TEST(test_wav_rejects_truncated_data);
    return UNITY_END();
}
