#include "nput/tokenizer.h"

#include "nput/internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    NPUT_TOKENIZER_MAX_ID = 65536,
    NPUT_TOKENIZER_MAX_TOKEN_CODEPOINTS = 256,
    NPUT_TOKENIZER_CODEPOINT_TABLE = 512,
    /* Whisper 的字节级词表到这里结束；更大的 id 是特殊 token 或时间戳 token，
       不应出现在转写文本中。 */
    NPUT_TOKENIZER_SPECIAL_FIRST = 50257
};

struct nput_tokenizer {
    uint8_t *bytes;      /* 拼接后的 UTF-8 token 字节 */
    size_t bytes_length;
    size_t bytes_capacity;
    uint32_t *offsets;   /* 每个 id 的字节偏移，0xFFFFFFFF 表示未使用 */
    uint16_t *lengths;   /* 每个 id 的字节长度 */
    size_t vocab_size;
    int16_t byte_of_codepoint[NPUT_TOKENIZER_CODEPOINT_TABLE];
};

static void build_byte_map(int16_t *byte_of_codepoint) {
    for (size_t i = 0; i < NPUT_TOKENIZER_CODEPOINT_TABLE; ++i) {
        byte_of_codepoint[i] = -1;
    }
    int extra = 0;
    for (int byte = 0; byte < 256; ++byte) {
        int codepoint;
        if ((byte >= 33 && byte <= 126) || (byte >= 161 && byte <= 172) || (byte >= 174 && byte <= 255)) {
            codepoint = byte;
        } else {
            codepoint = 256 + extra;
            ++extra;
        }
        if (codepoint < NPUT_TOKENIZER_CODEPOINT_TABLE) {
            byte_of_codepoint[codepoint] = (int16_t)byte;
        }
    }
}

static int decode_utf8(const unsigned char *text, size_t length, size_t *consumed, int32_t *codepoint) {
    if (length == 0) {
        return 0;
    }
    const unsigned char first = text[0];
    if (first < 0x80) {
        *consumed = 1;
        *codepoint = first;
        return 1;
    }
    size_t extra;
    int32_t value;
    if ((first & 0xE0) == 0xC0) {
        extra = 1;
        value = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        extra = 2;
        value = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        extra = 3;
        value = first & 0x07;
    } else {
        return 0;
    }
    if (length < extra + 1) {
        return 0;
    }
    for (size_t i = 1; i <= extra; ++i) {
        if ((text[i] & 0xC0) != 0x80) {
            return 0;
        }
        value = (value << 6) | (text[i] & 0x3F);
    }
    *consumed = extra + 1;
    *codepoint = value;
    return 1;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* 从起始引号开始解析 JSON 字符串，返回结束引号之后的位置；格式错误返回 NULL。 */
static const char *parse_json_string(const char *cursor, const char *end, int32_t *codepoints,
                                     size_t max_codepoints, size_t *out_count) {
    if (cursor >= end || *cursor != '"') {
        return NULL;
    }
    ++cursor;
    size_t count = 0;
    while (cursor < end) {
        const unsigned char current = (unsigned char)*cursor;
        if (current == '"') {
            *out_count = count;
            return cursor + 1;
        }
        if (current == '\\') {
            ++cursor;
            if (cursor >= end) {
                return NULL;
            }
            const char escape = *cursor++;
            int32_t value = 0;
            switch (escape) {
                case '"':
                case '\\':
                case '/':
                    value = escape;
                    break;
                case 'b':
                    value = '\b';
                    break;
                case 'f':
                    value = '\f';
                    break;
                case 'n':
                    value = '\n';
                    break;
                case 'r':
                    value = '\r';
                    break;
                case 't':
                    value = '\t';
                    break;
                case 'u': {
                    if (end - cursor < 4) {
                        return NULL;
                    }
                    for (int i = 0; i < 4; ++i) {
                        const int digit = hex_value(cursor[i]);
                        if (digit < 0) {
                            return NULL;
                        }
                        value = (value << 4) | digit;
                    }
                    cursor += 4;
                    break;
                }
                default:
                    return NULL;
            }
            if (count < max_codepoints) {
                codepoints[count] = value;
            }
            ++count;
            continue;
        }
        size_t consumed = 0;
        int32_t value = 0;
        if (!decode_utf8((const unsigned char *)cursor, (size_t)(end - cursor), &consumed, &value)) {
            return NULL;
        }
        cursor += consumed;
        if (count < max_codepoints) {
            codepoints[count] = value;
        }
        ++count;
    }
    return NULL;
}

static int append_byte(nput_tokenizer *tokenizer, uint8_t byte) {
    if (tokenizer->bytes_length == tokenizer->bytes_capacity) {
        size_t wanted = tokenizer->bytes_capacity == 0 ? 4096 : tokenizer->bytes_capacity * 2;
        uint8_t *grown = (uint8_t *)realloc(tokenizer->bytes, wanted);
        if (grown == NULL) {
            return 0;
        }
        tokenizer->bytes = grown;
        tokenizer->bytes_capacity = wanted;
    }
    tokenizer->bytes[tokenizer->bytes_length++] = byte;
    return 1;
}

static nput_status store_token(nput_tokenizer *tokenizer, const int32_t *codepoints, size_t count,
                               int64_t id) {
    if (id < 0 || id >= NPUT_TOKENIZER_MAX_ID) {
        return NPUT_OK;
    }
    const size_t start = tokenizer->bytes_length;
    for (size_t i = 0; i < count; ++i) {
        const int32_t codepoint = codepoints[i];
        if (codepoint < 0 || codepoint >= NPUT_TOKENIZER_CODEPOINT_TABLE) {
            return NPUT_ERR_FORMAT;
        }
        const int16_t byte = tokenizer->byte_of_codepoint[codepoint];
        if (byte < 0) {
            return NPUT_ERR_FORMAT;
        }
        if (!append_byte(tokenizer, (uint8_t)byte)) {
            return NPUT_ERR_OUT_OF_MEMORY;
        }
    }
    const size_t length = tokenizer->bytes_length - start;
    if (length > 0xFFFFu) {
        return NPUT_ERR_FORMAT;
    }
    tokenizer->offsets[id] = (uint32_t)start;
    tokenizer->lengths[id] = (uint16_t)length;
    if ((size_t)id + 1 > tokenizer->vocab_size) {
        tokenizer->vocab_size = (size_t)id + 1;
    }
    return NPUT_OK;
}

nput_status nput_tokenizer_load(const char *vocab_json_path, nput_tokenizer **out_tokenizer) {
    if (vocab_json_path == NULL || out_tokenizer == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_tokenizer = NULL;

    FILE *file = nput_file_open(vocab_json_path, "rb");
    if (file == NULL) {
        return NPUT_ERR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NPUT_ERR_IO;
    }
    const long file_size = ftell(file);
    if (file_size <= 0) {
        fclose(file);
        return NPUT_ERR_FORMAT;
    }
    rewind(file);
    char *document = (char *)malloc((size_t)file_size + 1);
    if (document == NULL) {
        fclose(file);
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    const size_t read = fread(document, 1, (size_t)file_size, file);
    fclose(file);
    document[read] = '\0';

    nput_tokenizer *tokenizer = (nput_tokenizer *)calloc(1, sizeof(nput_tokenizer));
    if (tokenizer == NULL) {
        free(document);
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    tokenizer->offsets = (uint32_t *)malloc(NPUT_TOKENIZER_MAX_ID * sizeof(uint32_t));
    tokenizer->lengths = (uint16_t *)calloc(NPUT_TOKENIZER_MAX_ID, sizeof(uint16_t));
    if (tokenizer->offsets == NULL || tokenizer->lengths == NULL) {
        free(document);
        nput_tokenizer_free(tokenizer);
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    memset(tokenizer->offsets, 0xFF, NPUT_TOKENIZER_MAX_ID * sizeof(uint32_t));
    build_byte_map(tokenizer->byte_of_codepoint);

    nput_status status = NPUT_OK;
    const char *cursor = document;
    const char *end = document + read;
    int32_t codepoints[NPUT_TOKENIZER_MAX_TOKEN_CODEPOINTS];

    while (cursor < end && status == NPUT_OK) {
        while (cursor < end && (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' ||
                                *cursor == '\t' || *cursor == ',' || *cursor == '{')) {
            ++cursor;
        }
        if (cursor >= end || *cursor == '}') {
            break;
        }
        size_t codepoint_count = 0;
        cursor = parse_json_string(cursor, end, codepoints, NPUT_TOKENIZER_MAX_TOKEN_CODEPOINTS,
                                   &codepoint_count);
        if (cursor == NULL) {
            status = NPUT_ERR_FORMAT;
            break;
        }
        if (codepoint_count > NPUT_TOKENIZER_MAX_TOKEN_CODEPOINTS) {
            status = NPUT_ERR_FORMAT;
            break;
        }
        while (cursor < end && (*cursor == ' ' || *cursor == ':')) {
            ++cursor;
        }
        int64_t id = 0;
        int digits = 0;
        while (cursor < end && *cursor >= '0' && *cursor <= '9') {
            id = id * 10 + (*cursor - '0');
            ++cursor;
            ++digits;
        }
        if (digits == 0) {
            status = NPUT_ERR_FORMAT;
            break;
        }
        status = store_token(tokenizer, codepoints, codepoint_count, id);
    }

    free(document);
    if (status != NPUT_OK || tokenizer->vocab_size == 0) {
        nput_tokenizer_free(tokenizer);
        return status == NPUT_OK ? NPUT_ERR_FORMAT : status;
    }
    *out_tokenizer = tokenizer;
    return NPUT_OK;
}

void nput_tokenizer_free(nput_tokenizer *tokenizer) {
    if (tokenizer == NULL) {
        return;
    }
    free(tokenizer->bytes);
    free(tokenizer->offsets);
    free(tokenizer->lengths);
    free(tokenizer);
}

nput_status nput_tokenizer_decode(const nput_tokenizer *tokenizer, const int64_t *ids, size_t count,
                                  char **out_text) {
    if (tokenizer == NULL || out_text == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    if (count > 0 && ids == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;

    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        const int64_t id = ids[i];
        if (id < 0 || id >= NPUT_TOKENIZER_SPECIAL_FIRST || (size_t)id >= tokenizer->vocab_size) {
            continue;
        }
        total += tokenizer->lengths[id];
    }

    char *text = (char *)malloc(total + 1);
    if (text == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    size_t written = 0;
    for (size_t i = 0; i < count; ++i) {
        const int64_t id = ids[i];
        if (id < 0 || id >= NPUT_TOKENIZER_SPECIAL_FIRST || (size_t)id >= tokenizer->vocab_size) {
            continue;
        }
        const size_t length = tokenizer->lengths[id];
        if (length == 0) {
            continue;
        }
        memcpy(text + written, tokenizer->bytes + tokenizer->offsets[id], length);
        written += length;
    }
    text[written] = '\0';
    *out_text = text;
    return NPUT_OK;
}

size_t nput_tokenizer_vocabulary_size(const nput_tokenizer *tokenizer) {
    return tokenizer == NULL ? 0 : tokenizer->vocab_size;
}

