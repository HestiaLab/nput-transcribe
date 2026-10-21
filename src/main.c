#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_args.h"
#include "cli_output.h"
#include "nput/asr.h"
#include "nput/audio.h"
#include "nput/mel.h"
#include "nput/model.h"
#include "nput/path.h"
#include "nput/segment.h"
#include "nput/status.h"

#ifndef NPUT_VERSION
#define NPUT_VERSION "0.0.0"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

#if defined(_WIN32)
/* Windows CRT 按 ANSI 代码页生成 argv/getenv，库则要求 UTF-8 路径，
 * 因此在 CLI 边界统一转换，库内部的 UTF-8 契约保持不变。 */
static char *utf8_from_wide(const wchar_t *wide) {
    const int length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (length <= 0) {
        return NULL;
    }
    char *utf8 = (char *)malloc((size_t)length);
    if (utf8 == NULL) {
        return NULL;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, length, NULL, NULL) == 0) {
        free(utf8);
        return NULL;
    }
    return utf8;
}

static int build_utf8_argv(int *argc, char ***argv) {
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), argc);
    if (wide == NULL) {
        return 0;
    }
    char **utf8 = (char **)calloc((size_t)*argc + 1, sizeof(char *));
    if (utf8 == NULL) {
        LocalFree(wide);
        return 0;
    }
    for (int i = 0; i < *argc; ++i) {
        utf8[i] = utf8_from_wide(wide[i]);
        if (utf8[i] == NULL) {
            for (int j = 0; j < i; ++j) {
                free(utf8[j]);
            }
            free(utf8);
            LocalFree(wide);
            return 0;
        }
    }
    LocalFree(wide);
    *argv = utf8;
    return 1;
}
#endif

/* 返回新分配的 UTF-8 环境变量值；未设置或为空时返回 NULL，调用方负责释放。 */
static char *read_env_utf8(const char *name) {
#if defined(_WIN32)
    wchar_t wide_name[64];
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, 64) == 0) {
        return NULL;
    }
    const wchar_t *value = _wgetenv(wide_name);
    if (value == NULL || value[0] == L'\0') {
        return NULL;
    }
    return utf8_from_wide(value);
#else
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') {
        return NULL;
    }
    const size_t length = strlen(value);
    char *copy = (char *)malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, value, length + 1);
    }
    return copy;
#endif
}

static void print_usage(FILE *stream) {
    fprintf(stream,
            "用法：nput-transcribe [选项] <音频文件>...\n"
            "\n"
            "使用 Whisper 转写 WAV/MP3 等音频。编码器默认运行在 Intel NPU 上，\n"
            "解码器留在 CPU，因为它的 KV 缓存是动态形状。\n"
            "\n"
            "选项：\n"
            "  -m, --model <目录>        OpenVINO Whisper IR 目录（默认自动查找）\n"
            "      --vocab <文件>        vocab.json 路径（默认：<model>/vocab.json）\n"
            "      --encoder-device <d>  编码器设备（默认：NPU）\n"
            "      --decoder-device <d>  解码器设备（默认：CPU）\n"
            "      --cache-dir <目录>    OpenVINO 编译缓存目录\n"
            "  -l, --language <代码>     语言代码（en/zh 等），auto 表示每个文件独立自动检测（默认：auto）\n"
            "  -f, --format <格式>       txt、srt 或 json（默认：txt）\n"
            "  -o, --output <路径>       写入文件，默认输出到标准输出\n"
            "      --max-tokens <n>      每 30 秒片段的 token 上限（默认：224）\n"
            "  -v, --verbose             在 stderr 打印每段耗时\n"
            "      --                    其后全部视为输入文件（可处理以 - 开头的文件名）\n"
            "      --list-devices        列出 OpenVINO 设备后退出\n"
            "      --list-models         列出可用的模型目录后退出\n"
            "      --version             打印版本后退出\n"
            "  -h, --help                打印本帮助后退出\n");
}

static void print_error_detail(void) {
    const char *detail = nput_last_error();
    if (detail != NULL && detail[0] != '\0') {
        fprintf(stderr, "  详情：%s\n", detail);
    }
}

/* 解析结果到 CLI 状态的桥接：立即退出的动作在这里执行，保持原有的退出语义。 */
static int parse_arguments(int argc, char **argv, cli_options *options, const char ***files,
                           size_t *file_count) {
    static cli_arguments arguments;
    cli_parse_arguments(argc, argv, &arguments);

    switch (arguments.action) {
        case CLI_ACTION_HELP:
            print_usage(stdout);
            exit(0);
        case CLI_ACTION_VERSION:
            printf("nput-transcribe %s\n", NPUT_VERSION);
            exit(0);
        case CLI_ACTION_LIST_DEVICES: {
            char *devices = NULL;
            if (nput_asr_devices(&devices) != NPUT_OK) {
                fprintf(stderr, "错误：无法查询 OpenVINO 设备\n");
                exit(1);
            }
            printf("%s\n", devices);
            nput_free(devices);
            exit(0);
        }
        case CLI_ACTION_LIST_MODELS: {
            char *env_model_dir = read_env_utf8("NPUT_MODEL_DIR");
            char *lines = NULL;
            const nput_status listed = nput_list_model_dirs(&lines);
            if (listed != NPUT_OK && !(listed == NPUT_ERR_UNSUPPORTED && env_model_dir != NULL)) {
                fprintf(stderr, "错误：未找到模型目录\n");
                free(env_model_dir);
                exit(1);
            }
            if (env_model_dir != NULL) {
                printf("%s\n", env_model_dir);
            }
            if (listed == NPUT_OK) {
                for (char *line = strtok(lines, "\n"); line != NULL; line = strtok(NULL, "\n")) {
                    if (env_model_dir != NULL && strcmp(line, env_model_dir) == 0) {
                        continue;
                    }
                    printf("%s\n", line);
                }
            }
            free(env_model_dir);
            nput_free(lines);
            exit(0);
        }
        case CLI_ACTION_ERROR:
            fprintf(stderr, "%s\n", arguments.error);
            return 0;
        case CLI_ACTION_RUN:
            break;
    }

    *options = arguments.options;
    *files = arguments.files;
    *file_count = arguments.file_count;
    return 1;
}

static char *trim_text(const char *text) {
    const size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    size_t written = 0;
    int pending_space = 0;
    size_t index = 0;
    while (index < length && (text[index] == ' ' || text[index] == '\t')) {
        ++index;
    }
    for (; index < length; ++index) {
        const char character = text[index];
        if (character == '\n' || character == '\r' || character == '\t') {
            pending_space = 1;
            continue;
        }
        if (pending_space) {
            if (written > 0 && character != ' ') {
                copy[written++] = ' ';
            }
            pending_space = 0;
        }
        copy[written++] = character;
    }
    while (written > 0 && copy[written - 1] == ' ') {
        --written;
    }
    copy[written] = '\0';
    return copy;
}

static nput_status transcribe_file(nput_asr *asr, const cli_options *options, const char *path,
                                   int32_t *language_token, file_result *result) {
    /* 失败路径也会进入输出阶段，路径必须在最早的位置记录。 */
    result->path = path;

    nput_audio decoded;
    nput_status status = nput_audio_load(path, &decoded);
    if (status != NPUT_OK) {
        fprintf(stderr, "错误：无法解码 %s：%s\n", path, nput_status_string(status));
        return status;
    }
    if (options->verbose) {
        fprintf(stderr, "[%s] 解码 %zu 个采样点，%d Hz（%.1f 秒）\n", path, decoded.count,
                decoded.sample_rate,
                decoded.sample_rate > 0 ? (double)decoded.count / (double)decoded.sample_rate : 0.0);
    }

    nput_audio audio;
    status = nput_audio_resample(&decoded, NPUT_MEL_RATE, &audio);
    nput_audio_free(&decoded);
    if (status != NPUT_OK) {
        fprintf(stderr, "错误：无法重采样 %s：%s\n", path, nput_status_string(status));
        return status;
    }
    if (options->verbose) {
        fprintf(stderr, "[%s] 重采样为 %zu 个采样点，%d Hz（%.1f 秒）\n", path, audio.count,
                audio.sample_rate, (double)audio.count / (double)NPUT_MEL_RATE);
    }

    nput_segment *segments = NULL;
    size_t segment_count = 0;
    status = nput_segment_split(audio.samples, audio.count, NPUT_MEL_RATE, NPUT_MEL_SAMPLES, &segments,
                                &segment_count);
    if (status != NPUT_OK) {
        fprintf(stderr, "错误：无法切分 %s：%s\n", path, nput_status_string(status));
        nput_audio_free(&audio);
        return status;
    }
    if (options->verbose) {
        fprintf(stderr, "[%s] %zu 个片段\n", path, segment_count);
    }

    result->segments = (segment_result *)calloc(segment_count == 0 ? 1 : segment_count,
                                                sizeof(segment_result));

    for (size_t i = 0; i < segment_count && result->segments != NULL; ++i) {
        const size_t begin = segments[i].begin;
        const size_t count = segments[i].end - segments[i].begin;
        int32_t language = *language_token;
        nput_clear_error();
        if (language < 0) {
            status = nput_asr_detect_language(asr, audio.samples + begin, count, &language);
            if (status == NPUT_OK) {
                *language_token = language;
            }
        }
        char *text = NULL;
        if (status == NPUT_OK) {
            status = nput_asr_transcribe(asr, audio.samples + begin, count, language, &text);
        }
        if (status != NPUT_OK) {
            break;
        }
        char *cleaned = trim_text(text);
        nput_free(text);
        if (cleaned == NULL) {
            status = NPUT_ERR_OUT_OF_MEMORY;
            break;
        }
        result->segments[result->count].start = (double)begin / (double)NPUT_MEL_RATE;
        result->segments[result->count].end =
            (double)segments[i].end / (double)NPUT_MEL_RATE;
        result->segments[result->count].text = cleaned;
        ++result->count;

        if (options->verbose) {
            nput_asr_timings timings;
            nput_asr_last_timings(asr, &timings);
            fprintf(stderr,
                    "[%s] 片段 %zu/%zu %.2f-%.2f 秒，编码 %.1f ms，解码 %.1f ms（%zu 个 token）\n",
                    path, i + 1, segment_count, result->segments[result->count - 1].start,
                    result->segments[result->count - 1].end, timings.encoder_ms, timings.decoder_ms,
                    timings.tokens);
        }
    }

    nput_segments_free(segments);
    nput_audio_free(&audio);
    if (result->segments == NULL) {
        fprintf(stderr, "错误：内存不足，无法保存 %s 的转写结果\n", path);
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    if (status != NPUT_OK) {
        fprintf(stderr, "错误：转写 %s 失败：%s\n", path, nput_status_string(status));
        print_error_detail();
    }
    return status;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    if (!build_utf8_argv(&argc, &argv)) {
        fprintf(stderr, "错误：无法解析命令行参数\n");
        return 1;
    }
#endif
    cli_options options;
    memset(&options, 0, sizeof(options));
    options.language = "auto";
    options.format = FORMAT_TXT;

    const char **files = NULL;
    size_t file_count = 0;
    if (!parse_arguments(argc, argv, &options, &files, &file_count)) {
        print_usage(stderr);
        return 2;
    }
    const char *model_dir = options.model_dir;
    char *env_model_dir = NULL;
    if (model_dir == NULL) {
        env_model_dir = read_env_utf8("NPUT_MODEL_DIR");
        model_dir = env_model_dir;
    }
    char *discovered_model_dir = NULL;
    if (model_dir == NULL) {
        if (nput_find_default_model_dir(&discovered_model_dir) != NPUT_OK) {
            fprintf(stderr,
                    "错误：未指定 --model，且未在 NPUT_MODEL_DIR、.\\models 或 Hugging Face "
                    "缓存中找到模型\n");
            return 2;
        }
        model_dir = discovered_model_dir;
        fprintf(stderr, "使用模型目录：%s\n", model_dir);
    }

    int32_t requested_language = nput_asr_language_token(options.language);
    if (requested_language < 0 && strcmp(options.language, "auto") != 0 &&
        strcmp(options.language, "detect") != 0) {
        fprintf(stderr, "错误：未知语言 '%s'\n", options.language);
        free(env_model_dir);
        free(discovered_model_dir);
        return 2;
    }

    if (options.output != NULL && options.output[0] != '\0' &&
        nput_create_parent_dirs(options.output) != NPUT_OK) {
        fprintf(stderr, "错误：无法创建输出目录：%s\n", options.output);
        free(env_model_dir);
        free(discovered_model_dir);
        return 1;
    }

    nput_asr_options asr_options;
    nput_asr_options_init(&asr_options);
    asr_options.model_dir = model_dir;
    asr_options.vocab_path = options.vocab_path;
    if (options.encoder_device != NULL) {
        asr_options.encoder_device = options.encoder_device;
    }
    if (options.decoder_device != NULL) {
        asr_options.decoder_device = options.decoder_device;
    }
    asr_options.cache_dir = options.cache_dir;
    if (options.max_tokens > 0) {
        asr_options.max_tokens = options.max_tokens;
    }
    asr_options.verbose = options.verbose;

    nput_asr *asr = NULL;
    nput_clear_error();
    nput_status status = nput_asr_create(&asr_options, &asr);
    if (status != NPUT_OK) {
        fprintf(stderr, "错误：无法初始化识别器：%s\n", nput_status_string(status));
        print_error_detail();
        free(env_model_dir);
        free(discovered_model_dir);
        return 1;
    }

    file_result *results = (file_result *)calloc(file_count, sizeof(file_result));
    if (results == NULL) {
        free(env_model_dir);
        free(discovered_model_dir);
        nput_asr_destroy(asr);
        return 1;
    }

    size_t completed = 0;
    int exit_code = 0;
    for (size_t i = 0; i < file_count; ++i) {
        /* 自动检测按文件独立进行，避免混合语言批次复用首个文件的结果。 */
        int32_t language_token = requested_language;
        status = transcribe_file(asr, &options, files[i], &language_token, &results[completed]);
        if (status != NPUT_OK) {
            exit_code = 1;
        }
        ++completed;
    }

    FILE *stream = stdout;
    if (options.output != NULL && options.output[0] != '\0') {
        stream = nput_file_open(options.output, "wb");
        if (stream == NULL) {
            fprintf(stderr, "错误：无法写入 %s\n", options.output);
            exit_code = 1;
        }
    }
    if (stream != NULL) {
        cli_write_results(stream, options.format, results, completed);
        if (stream == stdout) {
            if (fflush(stream) != 0 || ferror(stream) != 0) {
                fprintf(stderr, "错误：写入标准输出失败\n");
                exit_code = 1;
            }
        } else {
            const int write_failed = ferror(stream) != 0;
            const int close_failed = fclose(stream) != 0;
            if (write_failed || close_failed) {
                fprintf(stderr, "错误：写入 %s 失败\n", options.output);
                exit_code = 1;
            }
        }
    }

    for (size_t i = 0; i < completed; ++i) {
        for (size_t s = 0; s < results[i].count; ++s) {
            nput_free(results[i].segments[s].text);
        }
        free(results[i].segments);
    }
    free(results);
    nput_asr_destroy(asr);
    free(env_model_dir);
    free(discovered_model_dir);
    return exit_code;
}
