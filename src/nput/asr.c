#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "nput/asr.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#include <openvino/c/openvino.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nput/tokenizer.h"
#include "nput/trigram.h"

enum {
    NPUT_ASR_START_OF_TRANSCRIPT = 50258,
    NPUT_ASR_LANGUAGE_FIRST = 50259,
    NPUT_ASR_LANGUAGE_LAST = 50357,
    NPUT_ASR_TRANSCRIBE = 50359,
    NPUT_ASR_NO_TIMESTAMPS = 50363,
    NPUT_ASR_END_OF_TEXT = 50257,
    NPUT_ASR_HIDDEN_SIZE = 512,
    NPUT_ASR_ENCODER_FRAMES = 1500,
    NPUT_ASR_DEFAULT_MAX_TOKENS = 224,
    NPUT_ASR_PATH_MAX = 1024
};

struct nput_asr {
    ov_core_t *core;
    ov_compiled_model_t *encoder;
    ov_compiled_model_t *decoder;
    ov_infer_request_t *encoder_request;
    nput_tokenizer *tokenizer;
    int32_t max_tokens;
    int verbose;
    nput_asr_timings timings;
};

struct language_entry {
    const char *code;
    int32_t token;
};

static const struct language_entry NPUT_LANGUAGES[] = {
    {"en", 50259}, {"zh", 50260}, {"de", 50261}, {"es", 50262}, {"ru", 50263}, {"ko", 50264},
    {"fr", 50265}, {"ja", 50266}, {"pt", 50267}, {"tr", 50268}, {"pl", 50269}, {"ca", 50270},
    {"nl", 50271}, {"ar", 50272}, {"sv", 50273}, {"it", 50274}, {"id", 50275}, {"hi", 50276},
    {"fi", 50277}, {"vi", 50278}, {"he", 50279}, {"uk", 50280}, {"el", 50281}, {"ms", 50282},
    {"cs", 50283}, {"ro", 50284}, {"da", 50285}, {"hu", 50286}, {"ta", 50287}, {"no", 50288},
    {"th", 50289}, {"ur", 50290}, {"hr", 50291}, {"bg", 50292}, {"lt", 50293}, {"la", 50294},
    {"mi", 50295}, {"ml", 50296}, {"cy", 50297}, {"sk", 50298}, {"te", 50299}, {"fa", 50300},
    {"lv", 50301}, {"bn", 50302}, {"sr", 50303}, {"az", 50304}, {"sl", 50305}, {"kn", 50306},
    {"et", 50307}, {"mk", 50308}, {"br", 50309}, {"eu", 50310}, {"is", 50311}, {"hy", 50312},
    {"ne", 50313}, {"mn", 50314}, {"bs", 50315}, {"kk", 50316}, {"sq", 50317}, {"sw", 50318},
    {"gl", 50319}, {"mr", 50320}, {"pa", 50321}, {"si", 50322}, {"km", 50323}, {"sn", 50324},
    {"yo", 50325}, {"so", 50326}, {"af", 50327}, {"oc", 50328}, {"ka", 50329}, {"be", 50330},
    {"tg", 50331}, {"sd", 50332}, {"gu", 50333}, {"am", 50334}, {"yi", 50335}, {"lo", 50336},
    {"uz", 50337}, {"fo", 50338}, {"ht", 50339}, {"ps", 50340}, {"tk", 50341}, {"nn", 50342},
    {"mt", 50343}, {"sa", 50344}, {"lb", 50345}, {"my", 50346}, {"bo", 50347}, {"tl", 50348},
    {"mg", 50349}, {"as", 50350}, {"tt", 50351}, {"haw", 50352}, {"ln", 50353}, {"ha", 50354},
    {"ba", 50355}, {"jw", 50356}, {"su", 50357},
};

/* 单调时钟，单位为毫秒；不受系统校时影响。 */
static double monotonic_ms(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec * 1000.0 + (double)now.tv_nsec / 1e6;
#endif
}

/* 检查 OpenVINO 调用结果；失败时始终打印步骤名，避免默认模式下只剩笼统的错误。 */
static int openvino_step(ov_status_e status, const char *what) {
    if (status == OK) {
        return 1;
    }
    fprintf(stderr, "OpenVINO：%s 失败（状态码 %d）\n", what, (int)status);
    return 0;
}

int32_t nput_asr_language_token(const char *code) {
    if (code == NULL || code[0] == '\0') {
        return -1;
    }
    if (strcmp(code, "auto") == 0 || strcmp(code, "detect") == 0) {
        return -1;
    }
    for (size_t i = 0; i < sizeof(NPUT_LANGUAGES) / sizeof(NPUT_LANGUAGES[0]); ++i) {
        if (strcmp(NPUT_LANGUAGES[i].code, code) == 0) {
            return NPUT_LANGUAGES[i].token;
        }
    }
    return -1;
}

void nput_asr_options_init(nput_asr_options *options) {
    if (options == NULL) {
        return;
    }
    memset(options, 0, sizeof(*options));
    options->encoder_device = "NPU";
    options->decoder_device = "CPU";
    options->max_tokens = NPUT_ASR_DEFAULT_MAX_TOKENS;
}

static nput_status build_path(char *buffer, size_t size, const char *directory, const char *file) {
    const int written = snprintf(buffer, size, "%s/%s", directory, file);
    if (written < 0 || (size_t)written >= size) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    return NPUT_OK;
}

nput_status nput_asr_create(const nput_asr_options *options, nput_asr **out_asr) {
    if (options == NULL || out_asr == NULL || options->model_dir == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_asr = NULL;

    nput_asr *asr = (nput_asr *)calloc(1, sizeof(nput_asr));
    if (asr == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    asr->max_tokens = options->max_tokens > 0 ? options->max_tokens : NPUT_ASR_DEFAULT_MAX_TOKENS;
    asr->verbose = options->verbose;

    const char *encoder_device = options->encoder_device != NULL ? options->encoder_device : "NPU";
    const char *decoder_device = options->decoder_device != NULL ? options->decoder_device : "CPU";
    const char *cache_dir = options->cache_dir;

    char encoder_xml[NPUT_ASR_PATH_MAX];
    char decoder_xml[NPUT_ASR_PATH_MAX];
    char vocab_json[NPUT_ASR_PATH_MAX];

    nput_status status = build_path(encoder_xml, sizeof(encoder_xml), options->model_dir,
                                    "openvino_encoder_model.xml");
    if (status == NPUT_OK) {
        status = build_path(decoder_xml, sizeof(decoder_xml), options->model_dir,
                            "openvino_decoder_model.xml");
    }
    if (status == NPUT_OK) {
        if (options->vocab_path != NULL) {
            const int written = snprintf(vocab_json, sizeof(vocab_json), "%s", options->vocab_path);
            if (written < 0 || (size_t)written >= sizeof(vocab_json)) {
                status = NPUT_ERR_INVALID_ARGUMENT;
            }
        } else {
            status = build_path(vocab_json, sizeof(vocab_json), options->model_dir, "vocab.json");
        }
    }
    if (status != NPUT_OK) {
        nput_asr_destroy(asr);
        return status;
    }

    if (!openvino_step(ov_core_create(&asr->core), "创建 OpenVINO Core")) {
        nput_asr_destroy(asr);
        return NPUT_ERR_RUNTIME;
    }

    ov_model_t *encoder_model = NULL;
    if (!openvino_step(ov_core_read_model(asr->core, encoder_xml, NULL, &encoder_model), "读取编码器模型")) {
        nput_asr_destroy(asr);
        return NPUT_ERR_RUNTIME;
    }
    /* NPU 只接受静态形状；频谱固定为 80 x 3000。 */
    {
        int64_t dims[3] = {1, NPUT_MEL_BINS, NPUT_MEL_FRAMES};
        ov_partial_shape_t shape;
        if (ov_partial_shape_create_static(3, dims, &shape) != OK ||
            !openvino_step(ov_model_reshape_single_input(encoder_model, shape), "设置编码器输入形状")) {
            ov_model_free(encoder_model);
            nput_asr_destroy(asr);
            return NPUT_ERR_RUNTIME;
        }
        ov_partial_shape_free(&shape);
    }
    ov_status_e compile_status;
    if (cache_dir != NULL) {
        compile_status = ov_core_compile_model(asr->core, encoder_model, encoder_device, 2,
                                               &asr->encoder, "CACHE_DIR", cache_dir);
    } else {
        compile_status =
            ov_core_compile_model(asr->core, encoder_model, encoder_device, 0, &asr->encoder);
    }
    ov_model_free(encoder_model);
    if (!openvino_step(compile_status, "编译编码器")) {
        nput_asr_destroy(asr);
        return NPUT_ERR_RUNTIME;
    }

    ov_model_t *decoder_model = NULL;
    if (!openvino_step(ov_core_read_model(asr->core, decoder_xml, NULL, &decoder_model), "读取解码器模型")) {
        nput_asr_destroy(asr);
        return NPUT_ERR_RUNTIME;
    }
    const ov_status_e decoder_status =
        ov_core_compile_model(asr->core, decoder_model, decoder_device, 0, &asr->decoder);
    ov_model_free(decoder_model);
    if (!openvino_step(decoder_status, "编译解码器")) {
        nput_asr_destroy(asr);
        return NPUT_ERR_RUNTIME;
    }

    if (!openvino_step(ov_compiled_model_create_infer_request(asr->encoder, &asr->encoder_request), "创建编码器推理请求")) {
        nput_asr_destroy(asr);
        return NPUT_ERR_RUNTIME;
    }

    status = nput_tokenizer_load(vocab_json, &asr->tokenizer);
    if (status != NPUT_OK) {
        fprintf(stderr, "错误：无法加载词表 %s：%s\n", vocab_json, nput_status_string(status));
        nput_asr_destroy(asr);
        return status;
    }

    *out_asr = asr;
    return NPUT_OK;
}

void nput_asr_destroy(nput_asr *asr) {
    if (asr == NULL) {
        return;
    }
    if (asr->encoder_request != NULL) {
        ov_infer_request_free(asr->encoder_request);
    }
    if (asr->decoder != NULL) {
        ov_compiled_model_free(asr->decoder);
    }
    if (asr->encoder != NULL) {
        ov_compiled_model_free(asr->encoder);
    }
    if (asr->core != NULL) {
        ov_core_free(asr->core);
    }
    nput_tokenizer_free(asr->tokenizer);
    free(asr);
}

/* 运行编码器，把隐状态写入 `out_hidden`。返回的张量归编码器请求所有，
   在下一次调用前保持有效。 */
static nput_status run_encoder(nput_asr *asr, const nput_mel *mel, ov_tensor_t **out_hidden,
                               double *elapsed_ms) {
    if (mel == NULL || mel->data == NULL || mel->bins != NPUT_MEL_BINS ||
        mel->frames != NPUT_MEL_FRAMES) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    int64_t dims[3] = {1, NPUT_MEL_BINS, NPUT_MEL_FRAMES};
    ov_shape_t shape;
    if (ov_shape_create(3, dims, &shape) != OK) {
        return NPUT_ERR_RUNTIME;
    }
    ov_tensor_t *input = NULL;
    ov_status_e status =
        ov_tensor_create_from_host_ptr(F32, shape, (void *)mel->data, &input);
    ov_shape_free(&shape);
    if (!openvino_step(status, "创建编码器输入张量")) {
        return NPUT_ERR_RUNTIME;
    }

    const double start = monotonic_ms();
    int ok = openvino_step(ov_infer_request_set_tensor(asr->encoder_request, "input_features", input), "设置编码器输入") &&
             openvino_step(ov_infer_request_infer(asr->encoder_request), "运行编码器");
    *elapsed_ms = monotonic_ms() - start;
    ov_tensor_free(input);
    if (!ok) {
        return NPUT_ERR_RUNTIME;
    }

    ov_tensor_t *hidden = NULL;
    if (!openvino_step(ov_infer_request_get_tensor(asr->encoder_request, "last_hidden_state",
                                                   &hidden), "读取编码器输出")) {
        return NPUT_ERR_RUNTIME;
    }
    ov_shape_t hidden_shape;
    memset(&hidden_shape, 0, sizeof(hidden_shape));
    if (ov_tensor_get_shape(hidden, &hidden_shape) != OK) {
        ov_tensor_free(hidden);
        return NPUT_ERR_RUNTIME;
    }
    const int shape_ok = hidden_shape.rank == 3 &&
                         hidden_shape.dims[1] == NPUT_ASR_ENCODER_FRAMES &&
                         hidden_shape.dims[2] == NPUT_ASR_HIDDEN_SIZE;
    ov_shape_free(&hidden_shape);
    if (!shape_ok) {
        ov_tensor_free(hidden);
        return NPUT_ERR_RUNTIME;
    }
    *out_hidden = hidden;
    return NPUT_OK;
}

/* 语言检测与转写共用的贪心解码循环。
 *
 * `detect_only` 在确定语言 token 后立即返回；否则循环持续输出文本 token，
 * 并禁止已经出现过的 3-gram——这是 Whisper 长音频用来跳出音乐和噪声重复
 * 循环的保护规则。 */
static nput_status decode_loop(nput_asr *asr, ov_tensor_t *hidden, int32_t language_token,
                               int64_t *ids, size_t ids_capacity, size_t *out_count,
                               int32_t *out_language, int detect_only) {
    ov_infer_request_t *request = NULL;
    if (!openvino_step(ov_compiled_model_create_infer_request(asr->decoder, &request), "创建解码器推理请求")) {
        return NPUT_ERR_RUNTIME;
    }

    int64_t token_buffer[1] = {NPUT_ASR_START_OF_TRANSCRIPT};
    int32_t beam_buffer[1] = {0};
    int64_t token_dims[2] = {1, 1};
    int64_t beam_dims[1] = {1};
    ov_shape_t token_shape;
    ov_shape_t beam_shape;
    ov_tensor_t *token_tensor = NULL;
    ov_tensor_t *beam_tensor = NULL;
    nput_status result = NPUT_ERR_RUNTIME;

    if (ov_shape_create(2, token_dims, &token_shape) != OK ||
        ov_shape_create(1, beam_dims, &beam_shape) != OK) {
        goto cleanup;
    }
    const ov_status_e token_status =
        ov_tensor_create_from_host_ptr(I64, token_shape, token_buffer, &token_tensor);
    const ov_status_e beam_status =
        ov_tensor_create_from_host_ptr(I32, beam_shape, beam_buffer, &beam_tensor);
    ov_shape_free(&token_shape);
    ov_shape_free(&beam_shape);
    if (!openvino_step(token_status, "创建 token 张量") ||
        !openvino_step(beam_status, "创建 beam_idx 张量")) {
        goto cleanup;
    }

    if (!openvino_step(ov_infer_request_set_tensor(request, "input_ids", token_tensor), "设置 input_ids") ||
        !openvino_step(ov_infer_request_set_tensor(request, "encoder_hidden_states", hidden), "设置编码器隐状态") ||
        !openvino_step(ov_infer_request_set_tensor(request, "beam_idx", beam_tensor), "设置 beam_idx")) {
        goto cleanup;
    }

    enum { STAGE_LANGUAGE = 0, STAGE_TASK, STAGE_TIMESTAMPS, STAGE_TEXT };

    size_t count = 0;
    size_t steps = 0;
    int32_t detected = language_token;
    int stage = STAGE_LANGUAGE;
    for (;;) {
        if (!openvino_step(ov_infer_request_infer(request), "运行解码器")) {
            goto cleanup;
        }
        ov_tensor_t *logits = NULL;
        if (!openvino_step(ov_infer_request_get_tensor(request, "logits", &logits), "读取 logits")) {
            goto cleanup;
        }
        ov_shape_t logits_shape;
        if (ov_tensor_get_shape(logits, &logits_shape) != OK || logits_shape.rank != 3) {
            ov_tensor_free(logits);
            goto cleanup;
        }
        const int64_t vocabulary = logits_shape.dims[2];
        ov_shape_free(&logits_shape);
        void *raw = NULL;
        if (ov_tensor_data(logits, &raw) != OK || raw == NULL) {
            ov_tensor_free(logits);
            goto cleanup;
        }
        const float *scores = (const float *)raw;

        if (stage == STAGE_LANGUAGE) {
            if (detected < 0) {
                float best_score = -1e30f;
                for (int32_t id = NPUT_ASR_LANGUAGE_FIRST; id <= NPUT_ASR_LANGUAGE_LAST; ++id) {
                    if ((int64_t)id < vocabulary && scores[id] > best_score) {
                        best_score = scores[id];
                        detected = id;
                    }
                }
                if (detected < 0) {
                    ov_tensor_free(logits);
                    goto cleanup;
                }
            }
            if (detect_only) {
                ov_tensor_free(logits);
                *out_count = 0;
                if (out_language != NULL) {
                    *out_language = detected;
                }
                result = NPUT_OK;
                goto cleanup;
            }
            token_buffer[0] = detected;
            stage = STAGE_TASK;
            ov_tensor_free(logits);
            continue;
        }
        if (stage == STAGE_TASK) {
            token_buffer[0] = NPUT_ASR_TRANSCRIBE;
            stage = STAGE_TIMESTAMPS;
            ov_tensor_free(logits);
            continue;
        }
        if (stage == STAGE_TIMESTAMPS) {
            token_buffer[0] = NPUT_ASR_NO_TIMESTAMPS;
            stage = STAGE_TEXT;
            ov_tensor_free(logits);
            continue;
        }

        float best_score = -1e30f;
        int32_t best = 0;
        for (int64_t id = 0; id < vocabulary; ++id) {
            if (scores[id] > best_score) {
                best_score = scores[id];
                best = (int32_t)id;
            }
        }
        /* 当 (prev2, prev1, best) 这个 3-gram 已经出现时，禁止该候选。 */
        if (count >= 2 && nput_trigram_repeats(ids, count, best)) {
            int32_t rejected[8];
            size_t rejected_count = 0;
            int found = 0;
            while (!found) {
                rejected[rejected_count++] = best;
                float alternative_score = -1e30f;
                int32_t alternative = -1;
                for (int64_t id = 0; id < vocabulary; ++id) {
                    if (scores[id] <= alternative_score) {
                        continue;
                    }
                    int is_rejected = 0;
                    for (size_t r = 0; r < rejected_count; ++r) {
                        if (rejected[r] == (int32_t)id) {
                            is_rejected = 1;
                            break;
                        }
                    }
                    if (is_rejected) {
                        continue;
                    }
                    alternative_score = scores[id];
                    alternative = (int32_t)id;
                }
                if (alternative < 0) {
                    break; /* 候选都被排除，保留当前备选 token */
                }
                best = alternative;
                if (!nput_trigram_repeats(ids, count, best)) {
                    found = 1;
                }
                if (rejected_count == sizeof(rejected) / sizeof(rejected[0])) {
                    break;
                }
            }
        }
        ov_tensor_free(logits);

        if (best == NPUT_ASR_END_OF_TEXT) {
            break;
        }
        if (count < ids_capacity) {
            ids[count] = best;
            ++count;
        }
        ++steps;
        if (steps >= (size_t)asr->max_tokens) {
            break;
        }
        token_buffer[0] = best;
    }

    *out_count = count;
    if (out_language != NULL) {
        *out_language = detected;
    }
    result = NPUT_OK;

cleanup:
    if (token_tensor != NULL) {
        ov_tensor_free(token_tensor);
    }
    if (beam_tensor != NULL) {
        ov_tensor_free(beam_tensor);
    }
    ov_infer_request_free(request);
    return result;
}

nput_status nput_asr_detect_language(nput_asr *asr, const nput_mel *mel, int32_t *out_token) {
    if (asr == NULL || out_token == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    ov_tensor_t *hidden = NULL;
    double elapsed = 0.0;
    nput_status status = run_encoder(asr, mel, &hidden, &elapsed);
    if (status != NPUT_OK) {
        return status;
    }
    int64_t ids[1];
    size_t count = 0;
    int32_t detected = -1;
    status = decode_loop(asr, hidden, -1, ids, 1, &count, &detected, 1);
    ov_tensor_free(hidden);
    if (status == NPUT_OK) {
        *out_token = detected;
    }
    return status;
}

nput_status nput_asr_transcribe(nput_asr *asr, const nput_mel *mel, int32_t language_token,
                                char **out_text) {
    if (asr == NULL || out_text == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;

    ov_tensor_t *hidden = NULL;
    double encoder_ms = 0.0;
    nput_status status = run_encoder(asr, mel, &hidden, &encoder_ms);
    if (status != NPUT_OK) {
        return status;
    }

    const size_t capacity = (size_t)asr->max_tokens;
    int64_t *ids = (int64_t *)malloc(capacity * sizeof(int64_t));
    if (ids == NULL) {
        ov_tensor_free(hidden);
        return NPUT_ERR_OUT_OF_MEMORY;
    }

    const double decoder_start = monotonic_ms();
    size_t count = 0;
    status = decode_loop(asr, hidden, language_token, ids, capacity, &count, NULL, 0);
    asr->timings.decoder_ms = monotonic_ms() - decoder_start;
    asr->timings.encoder_ms = encoder_ms;
    asr->timings.tokens = count;
    ov_tensor_free(hidden);

    if (status == NPUT_OK) {
        char *raw_text = NULL;
        status = nput_tokenizer_decode(asr->tokenizer, ids, count, &raw_text);
        if (status == NPUT_OK) {
            /* Whisper 在第一个词前会输出一个空格。 */
            const char *begin = raw_text;
            while (*begin == ' ' || *begin == '\n' || *begin == '\t') {
                ++begin;
            }
            *out_text = (char *)malloc(strlen(begin) + 1);
            if (*out_text == NULL) {
                status = NPUT_ERR_OUT_OF_MEMORY;
            } else {
                memcpy(*out_text, begin, strlen(begin) + 1);
            }
            nput_free(raw_text);
        }
    }

    free(ids);
    return status;
}

void nput_asr_last_timings(const nput_asr *asr, nput_asr_timings *out_timings) {
    if (asr == NULL || out_timings == NULL) {
        return;
    }
    *out_timings = asr->timings;
}
