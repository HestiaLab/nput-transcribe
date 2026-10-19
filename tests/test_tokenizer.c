#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nput/tokenizer.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static const char *VOCAB_JSON =
    "{\"!\":0,\"\\\"\":1,\"\\u0120\":2,\"\\u00e4\\u00bd\\u0142\":3,\"\\u00e5\\u00a5\\u00bd\":4,"
    "\"<|endoftext|>\":50257}";

static nput_tokenizer *load_vocab(void) {
    const char *path = "nput_test_vocab.json";
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    fwrite(VOCAB_JSON, 1, strlen(VOCAB_JSON), file);
    fclose(file);

    nput_tokenizer *tokenizer = NULL;
    const nput_status status = nput_tokenizer_load(path, &tokenizer);
    remove(path);
    TEST_ASSERT_EQUAL(NPUT_OK, status);
    return tokenizer;
}

static void test_decode_byte_level_tokens(void) {
    nput_tokenizer *tokenizer = load_vocab();
    TEST_ASSERT_NOT_NULL(tokenizer);
    TEST_ASSERT_EQUAL_size_t(50258, nput_tokenizer_vocabulary_size(tokenizer));

    const int64_t ids[] = {0, 1, 2, 3, 4};
    char *text = NULL;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_tokenizer_decode(tokenizer, ids, 5, &text));
    TEST_ASSERT_NOT_NULL(text);
    /* '!'、'"'、空格，然后是两个字 token 的 UTF-8 字节（各 3 字节）。 */
    TEST_ASSERT_EQUAL_size_t(9, strlen(text));
    TEST_ASSERT_EQUAL_MEMORY("!\" ", text, 3);
    TEST_ASSERT_EQUAL_MEMORY("\xE4\xBD\xA0\xE5\xA5\xBD", text + 3, 6);
    nput_free(text);
    nput_tokenizer_free(tokenizer);
}

static void test_decode_skips_special_tokens(void) {
    nput_tokenizer *tokenizer = load_vocab();
    const int64_t ids[] = {3, 50257, 4};
    char *text = NULL;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_tokenizer_decode(tokenizer, ids, 3, &text));
    TEST_ASSERT_EQUAL_MEMORY("\xE4\xBD\xA0\xE5\xA5\xBD", text, 6);
    TEST_ASSERT_EQUAL_size_t(6, strlen(text));
    nput_free(text);
    nput_tokenizer_free(tokenizer);
}

static void test_decode_empty(void) {
    nput_tokenizer *tokenizer = load_vocab();
    char *text = NULL;
    TEST_ASSERT_EQUAL(NPUT_OK, nput_tokenizer_decode(tokenizer, NULL, 0, &text));
    TEST_ASSERT_EQUAL_STRING("", text);
    nput_free(text);
    nput_tokenizer_free(tokenizer);
}

static void test_missing_vocabulary(void) {
    nput_tokenizer *tokenizer = NULL;
    TEST_ASSERT_EQUAL(NPUT_ERR_IO, nput_tokenizer_load("nput_test_missing.json", &tokenizer));
    TEST_ASSERT_NULL(tokenizer);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_decode_byte_level_tokens);
    RUN_TEST(test_decode_skips_special_tokens);
    RUN_TEST(test_decode_empty);
    RUN_TEST(test_missing_vocabulary);
    return UNITY_END();
}
