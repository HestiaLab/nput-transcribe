#include "cli_output.h"

#include <stdio.h>

void cli_format_timestamp(char *buffer, size_t size, double seconds) {
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

/* 返回合法 UTF-8 序列的长度；非法或截断时返回 0。读取在 NUL 处短路。 */
static size_t utf8_sequence_length(const unsigned char *text) {
    const unsigned char first = text[0];
    if (first >= 0xC2 && first <= 0xDF) {
        return (text[1] & 0xC0) == 0x80 ? 2 : 0;
    }
    if (first == 0xE0) {
        return text[1] >= 0xA0 && text[1] <= 0xBF && (text[2] & 0xC0) == 0x80 ? 3 : 0;
    }
    if (first >= 0xE1 && first <= 0xEC) {
        return (text[1] & 0xC0) == 0x80 && (text[2] & 0xC0) == 0x80 ? 3 : 0;
    }
    if (first == 0xED) {
        return text[1] >= 0x80 && text[1] <= 0x9F && (text[2] & 0xC0) == 0x80 ? 3 : 0;
    }
    if (first >= 0xEE && first <= 0xEF) {
        return (text[1] & 0xC0) == 0x80 && (text[2] & 0xC0) == 0x80 ? 3 : 0;
    }
    if (first == 0xF0) {
        const int ok = text[1] >= 0x90 && text[1] <= 0xBF && (text[2] & 0xC0) == 0x80 &&
                       (text[3] & 0xC0) == 0x80;
        return ok ? 4 : 0;
    }
    if (first >= 0xF1 && first <= 0xF3) {
        const int ok = (text[1] & 0xC0) == 0x80 && (text[2] & 0xC0) == 0x80 &&
                       (text[3] & 0xC0) == 0x80;
        return ok ? 4 : 0;
    }
    if (first == 0xF4) {
        const int ok = text[1] >= 0x80 && text[1] <= 0x8F && (text[2] & 0xC0) == 0x80 &&
                       (text[3] & 0xC0) == 0x80;
        return ok ? 4 : 0;
    }
    return 0;
}

void cli_json_escape(FILE *stream, const char *text) {
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
                } else if (*cursor < 0x80) {
                    fputc(*cursor, stream);
                } else {
                    const size_t length = utf8_sequence_length(cursor);
                    if (length == 0) {
                        fputs("\xEF\xBF\xBD", stream);
                    } else {
                        fwrite(cursor, 1, length, stream);
                        cursor += length - 1;
                    }
                }
                break;
        }
    }
}

void cli_write_results(FILE *stream, output_format format, const file_result *results,
                       size_t result_count) {
    int first_entry = 1;
    size_t cue = 1;
    if (format == FORMAT_JSON) {
        fprintf(stream, "[\n");
    }
    for (size_t f = 0; f < result_count; ++f) {
        const file_result *result = &results[f];
        const char *path = result->path != NULL ? result->path : "";
        if (format == FORMAT_JSON) {
            if (!first_entry) {
                fprintf(stream, ",\n");
            }
            first_entry = 0;
            fprintf(stream, "  {\"file\": \"");
            cli_json_escape(stream, path);
            fprintf(stream, "\", \"segments\": [\n");
            for (size_t i = 0; i < result->count; ++i) {
                fprintf(stream, "    {\"start\": %.3f, \"end\": %.3f, \"text\": \"",
                        result->segments[i].start, result->segments[i].end);
                cli_json_escape(stream, result->segments[i].text);
                fprintf(stream, "\"}%s\n", i + 1 == result->count ? "" : ",");
            }
            fprintf(stream, "  ]}");
            continue;
        }
        if (result_count > 1) {
            if (format == FORMAT_TXT) {
                fprintf(stream, "%s==> %s <==\n", f == 0 ? "" : "\n", path);
            }
        }
        if (format == FORMAT_SRT) {
            for (size_t i = 0; i < result->count; ++i) {
                char start[32];
                char end[32];
                cli_format_timestamp(start, sizeof(start), result->segments[i].start);
                cli_format_timestamp(end, sizeof(end), result->segments[i].end);
                fprintf(stream, "%zu\n%s --> %s\n%s\n\n", cue++, start, end,
                        result->segments[i].text);
            }
        } else {
            for (size_t i = 0; i < result->count; ++i) {
                fprintf(stream, "%s\n", result->segments[i].text);
            }
        }
    }
    if (format == FORMAT_JSON) {
        fprintf(stream, "\n]\n");
    }
}
