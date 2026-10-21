#include "nput/asr.h"

#include <openvino/c/openvino.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asr/backend.h"
#include "core/error.h"
#include "nput/mel.h"
#include "nput/tokenizer.h"
#include "platform/clock.h"
#include "text/tokenizer_backend.h"
#include "text/trigram.h"

enum {
    NPUT_ASR_START_OF_TRANSCRIPT = 50258,
    NPUT_ASR_LANGUAGE_FIRST = 50259,
    NPUT_ASR_LANGUAGE_LAST = 50357,
    NPUT_ASR_TRANSCRIBE = 50359,
    NPUT_ASR_NO_TIMESTAMPS = 50363,
    NPUT_ASR_END_OF_TEXT = NPUT_TOKENIZER_SPECIAL_FIRST,
    /* 编码器卷积步长为 2；由 mel 帧数推导，不重复 1500 这个事实。 */
    WHISPER_ENCODER_FRAMES = NPUT_MEL_FRAMES / 2,
    NPUT_ASR_PATH_MAX = 1024
};

struct whisper_backend {
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
    {"en", 50259}, {"zh", 50260}, {"de", 50261}, {"es", 50262},  {"ru", 50263}, {"ko", 50264},
    {"fr", 50265}, {"ja", 50266}, {"pt", 50267}, {"tr", 50268},  {"pl", 50269}, {"ca", 50270},
    {"nl", 50271}, {"ar", 50272}, {"sv", 50273}, {"it", 50274},  {"id", 50275}, {"hi", 50276},
    {"fi", 50277}, {"vi", 50278}, {"he", 50279}, {"uk", 50280},  {"el", 50281}, {"ms", 50282},
    {"cs", 50283}, {"ro", 50284}, {"da", 50285}, {"hu", 50286},  {"ta", 50287}, {"no", 50288},
    {"th", 50289}, {"ur", 50290}, {"hr", 50291}, {"bg", 50292},  {"lt", 50293}, {"la", 50294},
    {"mi", 50295}, {"ml", 50296}, {"cy", 50297}, {"sk", 50298},  {"te", 50299}, {"fa", 50300},
    {"lv", 50301}, {"bn", 50302}, {"sr", 50303}, {"az", 50304},  {"sl", 50305}, {"kn", 50306},
    {"et", 50307}, {"mk", 50308}, {"br", 50309}, {"eu", 50310},  {"is", 50311}, {"hy", 50312},
    {"ne", 50313}, {"mn", 50314}, {"bs", 50315}, {"kk", 50316},  {"sq", 50317}, {"sw", 50318},
    {"gl", 50319}, {"mr", 50320}, {"pa", 50321}, {"si", 50322},  {"km", 50323}, {"sn", 50324},
    {"yo", 50325}, {"so", 50326}, {"af", 50327}, {"oc", 50328},  {"ka", 50329}, {"be", 50330},
    {"tg", 50331}, {"sd", 50332}, {"gu", 50333}, {"am", 50334},  {"yi", 50335}, {"lo", 50336},
    {"uz", 50337}, {"fo", 50338}, {"ht", 50339}, {"ps", 50340},  {"tk", 50341}, {"nn", 50342},
    {"mt", 50343}, {"sa", 50344}, {"lb", 50345}, {"my", 50346},  {"bo", 50347}, {"tl", 50348},
    {"mg", 50349}, {"as", 50350}, {"tt", 50351}, {"haw", 50352}, {"ln", 50353}, {"ha", 50354},
    {"ba", 50355}, {"jw", 50356}, {"su", 50357},
};

/* 检查 OpenVINO 调用结果；失败时记录步骤名，供调用方决定是否展示。 */
static int openvino_step(ov_status_e status, const char *what) {
    if (status == OK) {
        return 1;
    }
    nput_set_error("OpenVINO：%s 失败（状态码 %d）", what, (int)status);
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

static nput_status build_path(char *buffer, size_t size, const char *directory, const char *file) {
    const int written = snprintf(buffer, size, "%s/%s", directory, file);
    if (written < 0 || (size_t)written >= size) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    return NPUT_OK;
}

static void whisper_destroy(void *state);

/* 读取 IR 文件；失败时不产生需要释放的模型。 */
static nput_status read_model(ov_core_t *core, const char *path, const char *what,
                              ov_model_t **out_model) {
    if (!openvino_step(ov_core_read_model(core, path, NULL, out_model), what)) {
        return NPUT_ERR_RUNTIME;
    }
    return NPUT_OK;
}

/* 编译到目标设备；cache_dir 非空时启用编译器缓存。模型由调用方释放。 */
static nput_status compile_model(ov_core_t *core, ov_model_t *model, const char *device,
                                 const char *cache_dir, const char *what,
                                 ov_compiled_model_t **out_model) {
    ov_status_e status;
    if (cache_dir != NULL) {
        status = ov_core_compile_model(core, model, device, 2, out_model, "CACHE_DIR", cache_dir);
    } else {
        status = ov_core_compile_model(core, model, device, 0, out_model);
    }
    return openvino_step(status, what) ? NPUT_OK : NPUT_ERR_RUNTIME;
}

static nput_status whisper_create(const nput_asr_options *options, void **out_state) {
    if (options == NULL || out_state == NULL || options->model_dir == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_state = NULL;

    struct whisper_backend *asr = (struct whisper_backend *)calloc(1, sizeof(*asr));
    if (asr == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    asr->max_tokens = options->max_tokens;
    asr->verbose = options->verbose;

    const char *encoder_device = options->encoder_device;
    const char *decoder_device = options->decoder_device;
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
        whisper_destroy(asr);
        return status;
    }

    if (!openvino_step(ov_core_create(&asr->core), "创建 OpenVINO Core")) {
        whisper_destroy(asr);
        return NPUT_ERR_RUNTIME;
    }

    ov_model_t *encoder_model = NULL;
    status = read_model(asr->core, encoder_xml, "读取编码器模型", &encoder_model);
    if (status != NPUT_OK) {
        whisper_destroy(asr);
        return status;
    }
    /* NPU 只接受静态形状；频谱固定为 80 x 3000。 */
    {
        int64_t dims[3] = {1, NPUT_MEL_BINS, NPUT_MEL_FRAMES};
        ov_partial_shape_t shape;
        if (ov_partial_shape_create_static(3, dims, &shape) != OK) {
            ov_model_free(encoder_model);
            whisper_destroy(asr);
            return NPUT_ERR_RUNTIME;
        }
        const int reshaped = openvino_step(ov_model_reshape_single_input(encoder_model, shape),
                                           "设置编码器输入形状");
        ov_partial_shape_free(&shape);
        if (!reshaped) {
            ov_model_free(encoder_model);
            whisper_destroy(asr);
            return NPUT_ERR_RUNTIME;
        }
    }
    status = compile_model(asr->core, encoder_model, encoder_device, cache_dir, "编译编码器",
                           &asr->encoder);
    ov_model_free(encoder_model);
    if (status != NPUT_OK) {
        whisper_destroy(asr);
        return status;
    }

    ov_model_t *decoder_model = NULL;
    status = read_model(asr->core, decoder_xml, "读取解码器模型", &decoder_model);
    if (status == NPUT_OK) {
        status =
            compile_model(asr->core, decoder_model, decoder_device, NULL, "编译解码器", &asr->decoder);
    }
    if (decoder_model != NULL) {
        ov_model_free(decoder_model);
    }
    if (status != NPUT_OK) {
        whisper_destroy(asr);
        return status;
    }

    if (!openvino_step(ov_compiled_model_create_infer_request(asr->encoder, &asr->encoder_request),
                       "创建编码器推理请求")) {
        whisper_destroy(asr);
        return NPUT_ERR_RUNTIME;
    }

    status = nput_tokenizer_load(vocab_json, &asr->tokenizer);
    if (status != NPUT_OK) {
        nput_set_error("无法加载词表 %s：%s", vocab_json, nput_status_string(status));
        whisper_destroy(asr);
        return status;
    }

    *out_state = asr;
    return NPUT_OK;
}

static void whisper_destroy(void *state) {
    struct whisper_backend *asr = (struct whisper_backend *)state;
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
static nput_status run_encoder(struct whisper_backend *asr, const nput_mel *mel,
                               ov_tensor_t **out_hidden, double *elapsed_ms) {
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
    ov_status_e status = ov_tensor_create_from_host_ptr(F32, shape, (void *)mel->data, &input);
    ov_shape_free(&shape);
    if (!openvino_step(status, "创建编码器输入张量")) {
        return NPUT_ERR_RUNTIME;
    }

    const double start = nput_monotonic_ms();
    int ok =
        openvino_step(ov_infer_request_set_tensor(asr->encoder_request, "input_features", input),
                      "设置编码器输入") &&
        openvino_step(ov_infer_request_infer(asr->encoder_request), "运行编码器");
    *elapsed_ms = nput_monotonic_ms() - start;
    ov_tensor_free(input);
    if (!ok) {
        return NPUT_ERR_RUNTIME;
    }

    ov_tensor_t *hidden = NULL;
    if (!openvino_step(
            ov_infer_request_get_tensor(asr->encoder_request, "last_hidden_state", &hidden),
            "读取编码器输出")) {
        return NPUT_ERR_RUNTIME;
    }
    ov_shape_t hidden_shape;
    memset(&hidden_shape, 0, sizeof(hidden_shape));
    if (ov_tensor_get_shape(hidden, &hidden_shape) != OK) {
        ov_tensor_free(hidden);
        return NPUT_ERR_RUNTIME;
    }
    /* d_model 随模型规模变化（base 512 / small 768 / medium 1024），这里只校验
       编码器输出形状的前两维，隐藏维交给解码器消费。 */
    const int shape_ok = hidden_shape.rank == 3 &&
                         hidden_shape.dims[1] == WHISPER_ENCODER_FRAMES &&
                         hidden_shape.dims[2] > 0;
    ov_shape_free(&hidden_shape);
    if (!shape_ok) {
        ov_tensor_free(hidden);
        return NPUT_ERR_RUNTIME;
    }
    *out_hidden = hidden;
    return NPUT_OK;
}

/* 读取并校验 logits：形状必须是 1x1xV、元素类型必须是 F32。
 * 成功时 `*out_tensor` 由调用方释放。 */
static nput_status read_logits(ov_infer_request_t *request, ov_tensor_t **out_tensor,
                               const float **out_scores, int64_t *out_vocabulary) {
    ov_tensor_t *logits = NULL;
    if (!openvino_step(ov_infer_request_get_tensor(request, "logits", &logits), "读取 logits")) {
        return NPUT_ERR_RUNTIME;
    }
    ov_shape_t shape;
    if (!openvino_step(ov_tensor_get_shape(logits, &shape), "读取 logits 形状")) {
        ov_tensor_free(logits);
        return NPUT_ERR_RUNTIME;
    }
    if (shape.rank != 3) {
        nput_set_error("OpenVINO：logits 维度不是 3（实际为 %d）", (int)shape.rank);
        ov_shape_free(&shape);
        ov_tensor_free(logits);
        return NPUT_ERR_UNSUPPORTED;
    }
    if (shape.dims[0] != 1 || shape.dims[1] != 1) {
        nput_set_error("OpenVINO：logits 形状不是 1x1xV（dims[0]=%lld，dims[1]=%lld）",
                       (long long)shape.dims[0], (long long)shape.dims[1]);
        ov_shape_free(&shape);
        ov_tensor_free(logits);
        return NPUT_ERR_UNSUPPORTED;
    }
    const int64_t vocabulary = shape.dims[2];
    if (vocabulary <= 0 || vocabulary > INT32_MAX) {
        nput_set_error("OpenVINO：logits 形状不是 1x1xV（dims[2]=%lld）", (long long)vocabulary);
        ov_shape_free(&shape);
        ov_tensor_free(logits);
        return NPUT_ERR_UNSUPPORTED;
    }
    ov_shape_free(&shape);

    ov_element_type_e type = DYNAMIC;
    if (!openvino_step(ov_tensor_get_element_type(logits, &type), "读取 logits 元素类型")) {
        ov_tensor_free(logits);
        return NPUT_ERR_RUNTIME;
    }
    if (type != F32) {
        nput_set_error("OpenVINO：logits 元素类型不是 F32（实际为 %d）", (int)type);
        ov_tensor_free(logits);
        return NPUT_ERR_UNSUPPORTED;
    }
    void *raw = NULL;
    const ov_status_e data_status = ov_tensor_data(logits, &raw);
    if (data_status != OK) {
        nput_set_error("OpenVINO：读取 logits 数据失败（状态码 %d）", (int)data_status);
        ov_tensor_free(logits);
        return NPUT_ERR_RUNTIME;
    }
    if (raw == NULL) {
        nput_set_error("OpenVINO：读取 logits 数据返回空指针");
        ov_tensor_free(logits);
        return NPUT_ERR_RUNTIME;
    }

    *out_tensor = logits;
    *out_scores = (const float *)raw;
    *out_vocabulary = vocabulary;
    return NPUT_OK;
}

/* 语言检测与转写共用的贪心解码循环。
 *
 * `detect_only` 在确定语言 token 后立即返回；否则循环持续输出文本 token，
 * 并禁止已经出现过的 3-gram——这是 Whisper 长音频用来跳出音乐和噪声重复
 * 循环的保护规则。 */
static nput_status decode_loop(struct whisper_backend *asr, ov_tensor_t *hidden,
                               int32_t language_token, int64_t *ids, size_t ids_capacity,
                               size_t *out_count, int32_t *out_language, int detect_only) {
    ov_infer_request_t *request = NULL;
    if (!openvino_step(ov_compiled_model_create_infer_request(asr->decoder, &request),
                       "创建解码器推理请求")) {
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

    if (ov_shape_create(2, token_dims, &token_shape) != OK) {
        goto cleanup;
    }
    if (ov_shape_create(1, beam_dims, &beam_shape) != OK) {
        ov_shape_free(&token_shape);
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

    if (!openvino_step(ov_infer_request_set_tensor(request, "input_ids", token_tensor),
                       "设置 input_ids") ||
        !openvino_step(ov_infer_request_set_tensor(request, "encoder_hidden_states", hidden),
                       "设置编码器隐状态") ||
        !openvino_step(ov_infer_request_set_tensor(request, "beam_idx", beam_tensor),
                       "设置 beam_idx")) {
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
        const float *scores = NULL;
        int64_t vocabulary = 0;
        result = read_logits(request, &logits, &scores, &vocabulary);
        if (result != NPUT_OK) {
            goto cleanup;
        }

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

static nput_status whisper_detect_language(void *state, const float *samples, size_t count,
                                           int32_t *out_token) {
    struct whisper_backend *asr = (struct whisper_backend *)state;
    if (asr == NULL || out_token == NULL || samples == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }

    nput_mel mel;
    nput_status status = nput_mel_compute(samples, count, &mel);
    if (status != NPUT_OK) {
        return status;
    }

    ov_tensor_t *hidden = NULL;
    double elapsed = 0.0;
    status = run_encoder(asr, &mel, &hidden, &elapsed);
    nput_mel_free(&mel);
    if (status != NPUT_OK) {
        return status;
    }
    int64_t ids[1];
    size_t count_out = 0;
    int32_t detected = -1;
    status = decode_loop(asr, hidden, -1, ids, 1, &count_out, &detected, 1);
    ov_tensor_free(hidden);
    if (status == NPUT_OK) {
        *out_token = detected;
    }
    return status;
}

static nput_status whisper_transcribe(void *state, const float *samples, size_t count,
                                      int32_t language_token, char **out_text,
                                      nput_asr_timings *out_timings) {
    struct whisper_backend *asr = (struct whisper_backend *)state;
    if (asr == NULL || out_text == NULL || samples == NULL || out_timings == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;
    *out_timings = asr->timings;

    nput_mel mel;
    nput_status status = nput_mel_compute(samples, count, &mel);
    if (status != NPUT_OK) {
        return status;
    }

    ov_tensor_t *hidden = NULL;
    double encoder_ms = 0.0;
    status = run_encoder(asr, &mel, &hidden, &encoder_ms);
    nput_mel_free(&mel);
    if (status != NPUT_OK) {
        return status;
    }

    const size_t capacity = (size_t)asr->max_tokens;
    int64_t *ids = (int64_t *)malloc(capacity * sizeof(int64_t));
    if (ids == NULL) {
        ov_tensor_free(hidden);
        return NPUT_ERR_OUT_OF_MEMORY;
    }

    const double decoder_start = nput_monotonic_ms();
    size_t count_out = 0;
    status = decode_loop(asr, hidden, language_token, ids, capacity, &count_out, NULL, 0);
    asr->timings.decoder_ms = nput_monotonic_ms() - decoder_start;
    asr->timings.encoder_ms = encoder_ms;
    asr->timings.tokens = count_out;
    *out_timings = asr->timings;
    ov_tensor_free(hidden);

    if (status == NPUT_OK) {
        char *raw_text = NULL;
        status = nput_tokenizer_decode_below(asr->tokenizer, ids, count_out,
                                             NPUT_TOKENIZER_SPECIAL_FIRST, &raw_text);
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

const nput_asr_backend nput_asr_backend_whisper = {
    "whisper", whisper_create, whisper_detect_language, whisper_transcribe, whisper_destroy,
};
