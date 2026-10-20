#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nput/asr.h"
#include "nput/audio.h"
#include "nput/internal.h"
#include "nput/mel.h"
#include "nput/segment.h"
#include "nput/status.h"

#ifndef NPUT_VERSION
#define NPUT_VERSION "0.0.0"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

typedef enum output_format { FORMAT_TXT = 0, FORMAT_SRT, FORMAT_JSON } output_format;

typedef struct cli_options {
    const char *model_dir;
    const char *vocab_path;
    const char *encoder_device;
    const char *decoder_device;
    const char *cache_dir;
    const char *language;
    const char *output;
    output_format format;
    int32_t max_tokens;
    int verbose;
} cli_options;

typedef struct segment_result {
    double start;
    double end;
    char *text;
} segment_result;

typedef struct file_result {
    const char *path;
    segment_result *segments;
    size_t count;
} file_result;

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
            "  -l, --language <代码>     语言代码（en/zh 等），auto 表示自动检测（默认：auto）\n"
            "  -f, --format <格式>       txt、srt 或 json（默认：txt）\n"
            "  -o, --output <路径>       写入文件，默认输出到标准输出\n"
            "      --max-tokens <n>      每 30 秒片段的 token 上限（默认：224）\n"
            "  -v, --verbose             在 stderr 打印每段耗时\n"
            "      --list-devices        列出 OpenVINO 设备后退出\n"
            "      --list-models         列出可用的模型目录后退出\n"
            "      --version             打印版本后退出\n"
            "  -h, --help                打印本帮助后退出\n");
}

static int parse_int(const char *text, int32_t *out_value) {
    char *end = NULL;
    const long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return 0;
    }
    *out_value = (int32_t)value;
    return 1;
}

static int parse_arguments(int argc, char **argv, cli_options *options, const char ***files,
                           size_t *file_count) {
    static const char *positional[256];
    size_t positional_count = 0;

    for (int i = 1; i < argc; ++i) {
        const char *argument = argv[i];
        const char *value = NULL;
        const int has_next = i + 1 < argc;

        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            print_usage(stdout);
            exit(0);
        }
        if (strcmp(argument, "--version") == 0) {
            printf("nput-transcribe %s\n", NPUT_VERSION);
            exit(0);
        }
        if (strcmp(argument, "--list-devices") == 0) {
            char *devices = NULL;
            if (nput_asr_devices(&devices) != NPUT_OK) {
                fprintf(stderr, "错误：无法查询 OpenVINO 设备\n");
                exit(1);
            }
            printf("%s\n", devices);
            nput_free(devices);
            exit(0);
        }
        if (strcmp(argument, "--list-models") == 0) {
            char *lines = NULL;
            if (nput_list_model_dirs(&lines) != NPUT_OK) {
                fprintf(stderr, "错误：未找到模型目录\n");
                exit(1);
            }
            const char *env_model_dir = getenv("NPUT_MODEL_DIR");
            if (env_model_dir != NULL && env_model_dir[0] != '\0') {
                printf("%s\n", env_model_dir);
            }
            for (char *line = strtok(lines, "\n"); line != NULL; line = strtok(NULL, "\n")) {
                if (env_model_dir != NULL && strcmp(line, env_model_dir) == 0) {
                    continue;
                }
                printf("%s\n", line);
            }
            nput_free(lines);
            exit(0);
        }
        if (strcmp(argument, "-v") == 0 || strcmp(argument, "--verbose") == 0) {
            options->verbose = 1;
            continue;
        }

        if (strncmp(argument, "--", 2) == 0) {
            if (strchr(argument, '=') != NULL) {
                fprintf(stderr, "错误：%s 的取值需单独传入（不支持 = 形式）\n", argument);
                return 0;
            }
        }

        if (strcmp(argument, "-m") == 0 || strcmp(argument, "--model") == 0) {
            if (!has_next) {
                fprintf(stderr, "错误：%s 需要一个值\n", argument);
                return 0;
            }
            options->model_dir = argv[++i];
            continue;
        }
        if (strcmp(argument, "--vocab") == 0) {
            if (!has_next) {
                fprintf(stderr, "错误：%s 需要一个值\n", argument);
                return 0;
            }
            options->vocab_path = argv[++i];
            continue;
        }
        if (strcmp(argument, "--encoder-device") == 0) {
            if (!has_next) {
                fprintf(stderr, "错误：%s 需要一个值\n", argument);
                return 0;
            }
            options->encoder_device = argv[++i];
            continue;
        }
        if (strcmp(argument, "--decoder-device") == 0) {
            if (!has_next) {
                fprintf(stderr, "错误：%s 需要一个值\n", argument);
                return 0;
            }
            options->decoder_device = argv[++i];
            continue;
        }
        if (strcmp(argument, "--cache-dir") == 0) {
            if (!has_next) {
                fprintf(stderr, "错误：%s 需要一个值\n", argument);
                return 0;
            }
            options->cache_dir = argv[++i];
            continue;
        }
        if (strcmp(argument, "-l") == 0 || strcmp(argument, "--language") == 0) {
            if (!has_next) {
                fprintf(stderr, "错误：%s 需要一个值\n", argument);
                return 0;
            }
            options->language = argv[++i];
            continue;
        }
        if (strcmp(argument, "-o") == 0 || strcmp(argument, "--output") == 0) {
            if (!has_next) {
                fprintf(stderr, "错误：%s 需要一个值\n", argument);
                return 0;
            }
            options->output = argv[++i];
            continue;
        }
        if (strcmp(argument, "-f") == 0 || strcmp(argument, "--format") == 0) {
            if (!has_next) {
                fprintf(stderr, "错误：%s 需要一个值\n", argument);
                return 0;
            }
            value = argv[++i];
            if (strcmp(value, "txt") == 0) {
                options->format = FORMAT_TXT;
            } else if (strcmp(value, "srt") == 0) {
                options->format = FORMAT_SRT;
            } else if (strcmp(value, "json") == 0) {
                options->format = FORMAT_JSON;
            } else {
                fprintf(stderr, "错误：未知格式 '%s'\n", value);
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--max-tokens") == 0) {
            if (!has_next) {
                fprintf(stderr, "错误：%s 需要一个值\n", argument);
                return 0;
            }
            if (!parse_int(argv[++i], &options->max_tokens)) {
                fprintf(stderr, "错误：--max-tokens 需要一个数字\n");
                return 0;
            }
            continue;
        }
        if (argument[0] == '-' && argument[1] != '\0') {
            fprintf(stderr, "错误：未知选项 '%s'\n", argument);
            return 0;
        }
        if (positional_count == sizeof(positional) / sizeof(positional[0])) {
            fprintf(stderr, "错误：输入文件过多（最多 %zu 个）\n",
                    sizeof(positional) / sizeof(positional[0]));
            return 0;
        }
        positional[positional_count++] = argument;
    }

    if (positional_count == 0) {
        fprintf(stderr, "错误：未指定输入文件\n");
        return 0;
    }
    *files = positional;
    *file_count = positional_count;
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

static void format_timestamp(char *buffer, size_t size, double seconds) {
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    const int total_ms = (int)(seconds * 1000.0 + 0.5);
    const int hours = total_ms / 3600000;
    const int minutes = (total_ms / 60000) % 60;
    const int secs = (total_ms / 1000) % 60;
    const int millis = total_ms % 1000;
    snprintf(buffer, size, "%02d:%02d:%02d,%03d", hours, minutes, secs, millis);
}

static void json_escape(FILE *stream, const char *text) {
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; ++cursor) {
        switch (*cursor) {
            case '"':
                fputs("\\\"", stream);
                break;
            case '\\':
                fputs("\\\\", stream);
                break;
            case '\n':
                fputs("\\n", stream);
                break;
            case '\r':
                fputs("\\r", stream);
                break;
            case '\t':
                fputs("\\t", stream);
                break;
            default:
                if (*cursor < 0x20) {
                    fprintf(stream, "\\u%04x", *cursor);
                } else {
                    fputc(*cursor, stream);
                }
                break;
        }
    }
}

static nput_status transcribe_file(nput_asr *asr, const cli_options *options, const char *path,
                                   int32_t *language_token, file_result *result) {
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

    result->path = path;
    result->segments = (segment_result *)calloc(segment_count == 0 ? 1 : segment_count,
                                                sizeof(segment_result));

    for (size_t i = 0; i < segment_count && result->segments != NULL; ++i) {
        const size_t begin = segments[i].begin;
        const size_t count = segments[i].end - segments[i].begin;
        int32_t language = *language_token;
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
    }
    return status;
}

static void write_results(FILE *stream, const cli_options *options, const file_result *results,
                          size_t result_count) {
    int first_entry = 1;
    size_t cue = 1;
    if (options->format == FORMAT_JSON) {
        fprintf(stream, "[\n");
    }
    for (size_t f = 0; f < result_count; ++f) {
        const file_result *result = &results[f];
        if (options->format == FORMAT_JSON) {
            if (!first_entry) {
                fprintf(stream, ",\n");
            }
            first_entry = 0;
            fprintf(stream, "  {\"file\": \"");
            json_escape(stream, result->path);
            fprintf(stream, "\", \"segments\": [\n");
            for (size_t i = 0; i < result->count; ++i) {
                fprintf(stream, "    {\"start\": %.3f, \"end\": %.3f, \"text\": \"",
                        result->segments[i].start, result->segments[i].end);
                json_escape(stream, result->segments[i].text);
                fprintf(stream, "\"}%s\n", i + 1 == result->count ? "" : ",");
            }
            fprintf(stream, "  ]}");
            continue;
        }
        if (result_count > 1) {
            if (options->format == FORMAT_TXT) {
                fprintf(stream, "%s==> %s <==\n", f == 0 ? "" : "\n", result->path);
            }
        }
        if (options->format == FORMAT_SRT) {
            for (size_t i = 0; i < result->count; ++i) {
                char start[32];
                char end[32];
                format_timestamp(start, sizeof(start), result->segments[i].start);
                format_timestamp(end, sizeof(end), result->segments[i].end);
                fprintf(stream, "%zu\n%s --> %s\n%s\n\n", cue++, start, end,
                        result->segments[i].text);
            }
        } else {
            for (size_t i = 0; i < result->count; ++i) {
                fprintf(stream, "%s\n", result->segments[i].text);
            }
        }
    }
    if (options->format == FORMAT_JSON) {
        fprintf(stream, "\n]\n");
    }
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    cli_options options;
    memset(&options, 0, sizeof(options));
    options.encoder_device = "NPU";
    options.decoder_device = "CPU";
    options.language = "auto";
    options.format = FORMAT_TXT;
    options.max_tokens = 224;

    const char **files = NULL;
    size_t file_count = 0;
    if (!parse_arguments(argc, argv, &options, &files, &file_count)) {
        print_usage(stderr);
        return 2;
    }
    const char *model_dir = options.model_dir;
    if (model_dir == NULL) {
        const char *env_model_dir = getenv("NPUT_MODEL_DIR");
        if (env_model_dir != NULL && env_model_dir[0] != '\0') {
            model_dir = env_model_dir;
        }
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

    int32_t language_token = nput_asr_language_token(options.language);
    if (language_token < 0 && strcmp(options.language, "auto") != 0 &&
        strcmp(options.language, "detect") != 0) {
        fprintf(stderr, "错误：未知语言 '%s'\n", options.language);
        free(discovered_model_dir);
        return 2;
    }

    if (options.output != NULL && options.output[0] != '\0' &&
        nput_create_parent_dirs(options.output) != NPUT_OK) {
        fprintf(stderr, "错误：无法创建输出目录：%s\n", options.output);
        free(discovered_model_dir);
        return 1;
    }

    nput_asr_options asr_options;
    nput_asr_options_init(&asr_options);
    asr_options.model_dir = model_dir;
    asr_options.vocab_path = options.vocab_path;
    asr_options.encoder_device = options.encoder_device;
    asr_options.decoder_device = options.decoder_device;
    asr_options.cache_dir = options.cache_dir;
    asr_options.max_tokens = options.max_tokens;
    asr_options.verbose = options.verbose;

    nput_asr *asr = NULL;
    nput_status status = nput_asr_create(&asr_options, &asr);
    if (status != NPUT_OK) {
        fprintf(stderr, "错误：无法初始化识别器：%s\n", nput_status_string(status));
        free(discovered_model_dir);
        return 1;
    }

    file_result *results = (file_result *)calloc(file_count, sizeof(file_result));
    if (results == NULL) {
        free(discovered_model_dir);
        nput_asr_destroy(asr);
        return 1;
    }

    size_t completed = 0;
    int exit_code = 0;
    for (size_t i = 0; i < file_count; ++i) {
        status = transcribe_file(asr, &options, files[i], &language_token, &results[completed]);
        if (status != NPUT_OK) {
            exit_code = 1;
        }
        ++completed;
    }

    FILE *stream = stdout;
    if (options.output != NULL && options.output[0] != '\0') {
        stream = fopen(options.output, "wb");
        if (stream == NULL) {
            fprintf(stderr, "错误：无法写入 %s\n", options.output);
            exit_code = 1;
        }
    }
    if (stream != NULL) {
        write_results(stream, &options, results, completed);
        if (stream != stdout) {
            if (ferror(stream) != 0 || fclose(stream) != 0) {
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
    free(discovered_model_dir);
    return exit_code;
}
