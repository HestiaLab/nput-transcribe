#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include "nput/model.h"
#include "nput/path.h"

#include <stdlib.h>
#include <string.h>

#define NPUT_PATH_CHARS 2048

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

FILE *nput_file_open(const char *utf8_path, const char *mode) {
    wchar_t wide_path[1024];
    wchar_t wide_mode[16];
    if (utf8_path == NULL || mode == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, wide_path, 1024) == 0) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wide_mode, 16) == 0) {
        return NULL;
    }
    return _wfopen(wide_path, wide_mode);
}

nput_status nput_create_parent_dirs(const char *utf8_path) {
    if (utf8_path == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    const size_t length = strlen(utf8_path);
    if (length == 0) {
        return NPUT_OK;
    }
    wchar_t *wide = (wchar_t *)malloc((length + 1) * sizeof(wchar_t));
    if (wide == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, wide, (int)(length + 1)) == 0) {
        free(wide);
        return NPUT_ERR_INVALID_ARGUMENT;
    }

    size_t start = 0;
    if (wide[0] == L'\\' && wide[1] == L'\\') {
        /* UNC 路径：跳过 \\server\share，共享目录由系统管理。 */
        size_t separators = 0;
        start = 2;
        while (wide[start] != L'\0' && separators < 2) {
            if (wide[start] == L'\\' || wide[start] == L'/') {
                ++separators;
            }
            ++start;
        }
    } else if (wide[1] == L':') {
        start = 2; /* 跳过盘符，例如 C: */
    }

    nput_status status = NPUT_OK;
    const size_t wide_length = wcslen(wide);
    for (size_t i = start + 1; i < wide_length; ++i) {
        if (wide[i] != L'\\' && wide[i] != L'/') {
            continue;
        }
        if (wide[i - 1] == L'\\' || wide[i - 1] == L'/') {
            continue;
        }
        const wchar_t saved = wide[i];
        wide[i] = L'\0';
        if (!CreateDirectoryW(wide, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            wide[i] = saved;
            status = NPUT_ERR_IO;
            break;
        }
        wide[i] = saved;
    }
    free(wide);
    return status;
}

static int nput_join_wide(wchar_t *out, size_t size, const wchar_t *left, const wchar_t *right) {
    const size_t left_length = wcslen(left);
    const size_t right_length = wcslen(right);
    if (left_length + right_length + 1 > size) {
        return 0;
    }
    memcpy(out, left, left_length * sizeof(wchar_t));
    memcpy(out + left_length, right, (right_length + 1) * sizeof(wchar_t));
    return 1;
}

static int nput_join_wide3(wchar_t *out, size_t size, const wchar_t *first, const wchar_t *second,
                           const wchar_t *third) {
    if (!nput_join_wide(out, size, first, second)) {
        return 0;
    }
    return nput_join_wide(out + wcslen(out), size - wcslen(out), L"", third);
}

static int nput_wide_has_model(const wchar_t *directory) {
    static const wchar_t *const names[] = {
        L"\\openvino_encoder_model.xml",
        L"\\openvino_decoder_model.xml",
        L"\\vocab.json",
    };
    wchar_t path[NPUT_PATH_CHARS];
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (!nput_join_wide(path, NPUT_PATH_CHARS, directory, names[i]) ||
            GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
            return 0;
        }
    }
    return 1;
}

typedef struct nput_model_candidate {
    wchar_t *path;
    FILETIME time;
} nput_model_candidate;

/* 记录包含完整 IR 的目录。 */
static nput_status nput_consider_model_dir(const wchar_t *directory, nput_model_candidate **items,
                                           size_t *count, size_t *capacity) {
    wchar_t probe[NPUT_PATH_CHARS];
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!nput_wide_has_model(directory) ||
        !nput_join_wide(probe, NPUT_PATH_CHARS, directory, L"\\openvino_encoder_model.xml") ||
        !GetFileAttributesExW(probe, GetFileExInfoStandard, &data)) {
        return NPUT_OK;
    }
    if (*count == *capacity) {
        const size_t wanted = *capacity == 0 ? 8 : *capacity * 2;
        nput_model_candidate *grown =
            (nput_model_candidate *)realloc(*items, wanted * sizeof(nput_model_candidate));
        if (grown == NULL) {
            return NPUT_ERR_OUT_OF_MEMORY;
        }
        *items = grown;
        *capacity = wanted;
    }
    const size_t length = wcslen(directory);
    wchar_t *copy = (wchar_t *)malloc((length + 1) * sizeof(wchar_t));
    if (copy == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    memcpy(copy, directory, (length + 1) * sizeof(wchar_t));
    (*items)[*count].path = copy;
    (*items)[*count].time = data.ftLastWriteTime;
    ++(*count);
    return NPUT_OK;
}

static int nput_compare_candidates(const void *left, const void *right) {
    const nput_model_candidate *a = (const nput_model_candidate *)left;
    const nput_model_candidate *b = (const nput_model_candidate *)right;
    return CompareFileTime(&b->time, &a->time); /* 新的在前 */
}

static void nput_free_candidates(nput_model_candidate *items, size_t count);

static nput_status nput_collect_models(nput_model_candidate **out_items, size_t *out_count) {
    nput_model_candidate *items = NULL;
    size_t count = 0;
    size_t capacity = 0;

    /* 1. 当前目录下的 ./models。 */
    nput_status status = nput_consider_model_dir(L"models", &items, &count, &capacity);

    /* 2. Hugging Face 缓存：<HF_HOME>/hub 或 ~/.cache/huggingface/hub。 */
    wchar_t root[NPUT_PATH_CHARS];
    root[0] = L'\0';
    const wchar_t *hf_home = _wgetenv(L"HF_HOME");
    if (hf_home != NULL && hf_home[0] != L'\0') {
        nput_join_wide(root, NPUT_PATH_CHARS, hf_home, L"\\hub");
    } else {
        const wchar_t *profile = _wgetenv(L"USERPROFILE");
        if (profile != NULL) {
            nput_join_wide(root, NPUT_PATH_CHARS, profile, L"\\.cache\\huggingface\\hub");
        }
    }

    wchar_t pattern[NPUT_PATH_CHARS];
    if (status == NPUT_OK && root[0] != L'\0' &&
        nput_join_wide(pattern, NPUT_PATH_CHARS, root, L"\\models--*whisper*")) {
        WIN32_FIND_DATAW model_entry;
        HANDLE models = FindFirstFileW(pattern, &model_entry);
        if (models != INVALID_HANDLE_VALUE) {
            do {
                if (status != NPUT_OK ||
                    (model_entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                    continue;
                }
                wchar_t model_dir[NPUT_PATH_CHARS];
                wchar_t snapshot_pattern[NPUT_PATH_CHARS];
                if (!nput_join_wide3(model_dir, NPUT_PATH_CHARS, root, L"\\", model_entry.cFileName) ||
                    !nput_join_wide(snapshot_pattern, NPUT_PATH_CHARS, model_dir, L"\\snapshots\\*")) {
                    continue;
                }
                WIN32_FIND_DATAW snapshot_entry;
                HANDLE snapshots = FindFirstFileW(snapshot_pattern, &snapshot_entry);
                if (snapshots == INVALID_HANDLE_VALUE) {
                    continue;
                }
                do {
                    if (status != NPUT_OK ||
                        (snapshot_entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                        continue;
                    }
                    wchar_t snapshot_dir[NPUT_PATH_CHARS];
                    if (nput_join_wide3(snapshot_dir, NPUT_PATH_CHARS, model_dir, L"\\snapshots\\",
                                        snapshot_entry.cFileName)) {
                        status = nput_consider_model_dir(snapshot_dir, &items, &count, &capacity);
                    }
                } while (status == NPUT_OK && FindNextFileW(snapshots, &snapshot_entry));
                FindClose(snapshots);
            } while (status == NPUT_OK && FindNextFileW(models, &model_entry));
            FindClose(models);
        }
    }

    if (status != NPUT_OK) {
        nput_free_candidates(items, count);
        return status;
    }
    qsort(items, count, sizeof(nput_model_candidate), nput_compare_candidates);
    *out_items = items;
    *out_count = count;
    return NPUT_OK;
}

static void nput_free_candidates(nput_model_candidate *items, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        free(items[i].path);
    }
    free(items);
}

static nput_status nput_wide_to_utf8(const wchar_t *text, char **out_utf8) {
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (bytes <= 0) {
        return NPUT_ERR_RUNTIME;
    }
    char *utf8 = (char *)malloc((size_t)bytes);
    if (utf8 == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, bytes, NULL, NULL) == 0) {
        free(utf8);
        return NPUT_ERR_RUNTIME;
    }
    *out_utf8 = utf8;
    return NPUT_OK;
}

nput_status nput_find_default_model_dir(char **out_path) {
    if (out_path == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_path = NULL;

    nput_model_candidate *items = NULL;
    size_t count = 0;
    nput_status status = nput_collect_models(&items, &count);
    if (status == NPUT_OK) {
        status = count > 0 ? nput_wide_to_utf8(items[0].path, out_path) : NPUT_ERR_UNSUPPORTED;
    }
    nput_free_candidates(items, count);
    return status;
}

nput_status nput_list_model_dirs(char **out_lines) {
    if (out_lines == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_lines = NULL;

    nput_model_candidate *items = NULL;
    size_t count = 0;
    nput_status status = nput_collect_models(&items, &count);
    if (status != NPUT_OK) {
        return status;
    }
    if (count == 0) {
        nput_free_candidates(items, count);
        return NPUT_ERR_UNSUPPORTED;
    }

    char **encoded = (char **)calloc(count, sizeof(char *));
    if (encoded == NULL) {
        nput_free_candidates(items, count);
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    size_t total = 1;
    for (size_t i = 0; i < count; ++i) {
        status = nput_wide_to_utf8(items[i].path, &encoded[i]);
        if (status != NPUT_OK) {
            break;
        }
        total += strlen(encoded[i]) + 1;
    }

    char *lines = status == NPUT_OK ? (char *)malloc(total) : NULL;
    if (status == NPUT_OK && lines == NULL) {
        status = NPUT_ERR_OUT_OF_MEMORY;
    }
    if (status == NPUT_OK) {
        size_t offset = 0;
        for (size_t i = 0; i < count; ++i) {
            const size_t length = strlen(encoded[i]);
            memcpy(lines + offset, encoded[i], length);
            offset += length;
            lines[offset++] = (i + 1 == count) ? '\0' : '\n';
        }
        *out_lines = lines;
    }

    for (size_t i = 0; i < count; ++i) {
        free(encoded[i]);
    }
    free(encoded);
    nput_free_candidates(items, count);
    return status;
}
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

FILE *nput_file_open(const char *utf8_path, const char *mode) {
    return fopen(utf8_path, mode);
}

nput_status nput_create_parent_dirs(const char *utf8_path) {
    if (utf8_path == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    const size_t length = strlen(utf8_path);
    if (length == 0) {
        return NPUT_OK;
    }
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    memcpy(copy, utf8_path, length + 1);

    nput_status status = NPUT_OK;
    for (size_t i = 1; i < length; ++i) {
        if (copy[i] != '/' || copy[i - 1] == '/') {
            continue;
        }
        copy[i] = '\0';
        if (mkdir(copy, 0777) != 0 && errno != EEXIST) {
            copy[i] = '/';
            status = NPUT_ERR_IO;
            break;
        }
        copy[i] = '/';
    }
    free(copy);
    return status;
}

static int nput_posix_has_model(const char *directory) {
    static const char *const names[] = {
        "/openvino_encoder_model.xml",
        "/openvino_decoder_model.xml",
        "/vocab.json",
    };
    char path[NPUT_PATH_CHARS];
    struct stat info;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (snprintf(path, sizeof(path), "%s%s", directory, names[i]) <= 0 ||
            stat(path, &info) != 0) {
            return 0;
        }
    }
    return 1;
}

typedef struct nput_posix_candidate {
    char *path;
    time_t time;
} nput_posix_candidate;

static nput_status nput_consider_posix_dir(const char *directory, nput_posix_candidate **items,
                                           size_t *count, size_t *capacity) {
    char path[NPUT_PATH_CHARS];
    struct stat info;
    if (!nput_posix_has_model(directory) ||
        snprintf(path, sizeof(path), "%s/openvino_encoder_model.xml", directory) <= 0 ||
        stat(path, &info) != 0) {
        return NPUT_OK;
    }
    if (*count == *capacity) {
        const size_t wanted = *capacity == 0 ? 8 : *capacity * 2;
        nput_posix_candidate *grown =
            (nput_posix_candidate *)realloc(*items, wanted * sizeof(nput_posix_candidate));
        if (grown == NULL) {
            return NPUT_ERR_OUT_OF_MEMORY;
        }
        *items = grown;
        *capacity = wanted;
    }
    const size_t length = strlen(directory);
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    memcpy(copy, directory, length + 1);
    (*items)[*count].path = copy;
    (*items)[*count].time = info.st_mtime;
    ++(*count);
    return NPUT_OK;
}

static int nput_compare_posix_candidates(const void *left, const void *right) {
    const nput_posix_candidate *a = (const nput_posix_candidate *)left;
    const nput_posix_candidate *b = (const nput_posix_candidate *)right;
    if (a->time == b->time) {
        return 0;
    }
    return a->time < b->time ? 1 : -1; /* 新的在前 */
}

static nput_status nput_collect_models(nput_posix_candidate **out_items, size_t *out_count) {
    nput_posix_candidate *items = NULL;
    size_t count = 0;
    size_t capacity = 0;

    nput_status status = nput_consider_posix_dir("models", &items, &count, &capacity);

    char root[NPUT_PATH_CHARS];
    root[0] = '\0';
    const char *hf_home = getenv("HF_HOME");
    if (hf_home != NULL && hf_home[0] != '\0') {
        snprintf(root, sizeof(root), "%s/hub", hf_home);
    } else {
        const char *home = getenv("HOME");
        if (home != NULL) {
            snprintf(root, sizeof(root), "%s/.cache/huggingface/hub", home);
        }
    }

    DIR *models = status == NPUT_OK && root[0] != '\0' ? opendir(root) : NULL;
    if (models != NULL) {
        struct dirent *entry;
        while ((entry = readdir(models)) != NULL) {
            if (strncmp(entry->d_name, "models--", 8) != 0 ||
                strstr(entry->d_name, "whisper") == NULL) {
                continue;
            }
            char snapshots[NPUT_PATH_CHARS];
            snprintf(snapshots, sizeof(snapshots), "%s/%s/snapshots", root, entry->d_name);
            DIR *snapshot_dir = opendir(snapshots);
            if (snapshot_dir == NULL) {
                continue;
            }
            struct dirent *snapshot;
            while ((snapshot = readdir(snapshot_dir)) != NULL) {
                if (snapshot->d_name[0] == '.') {
                    continue;
                }
                char candidate[NPUT_PATH_CHARS];
                snprintf(candidate, sizeof(candidate), "%s/%s", snapshots, snapshot->d_name);
                const nput_status candidate_status =
                    nput_consider_posix_dir(candidate, &items, &count, &capacity);
                if (candidate_status != NPUT_OK) {
                    status = candidate_status;
                    break;
                }
            }
            closedir(snapshot_dir);
            if (status != NPUT_OK) {
                break;
            }
        }
        closedir(models);
    }

    if (status == NPUT_OK) {
        qsort(items, count, sizeof(nput_posix_candidate), nput_compare_posix_candidates);
        *out_items = items;
        *out_count = count;
        return NPUT_OK;
    }
    for (size_t i = 0; i < count; ++i) {
        free(items[i].path);
    }
    free(items);
    return status;
}

static void nput_free_posix_candidates(nput_posix_candidate *items, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        free(items[i].path);
    }
    free(items);
}

nput_status nput_find_default_model_dir(char **out_path) {
    if (out_path == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_path = NULL;

    nput_posix_candidate *items = NULL;
    size_t count = 0;
    nput_status status = nput_collect_models(&items, &count);
    if (status != NPUT_OK) {
        return status;
    }
    if (count == 0) {
        nput_free_posix_candidates(items, count);
        return NPUT_ERR_UNSUPPORTED;
    }
    const size_t length = strlen(items[0].path);
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        nput_free_posix_candidates(items, count);
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    memcpy(copy, items[0].path, length + 1);
    *out_path = copy;
    nput_free_posix_candidates(items, count);
    return NPUT_OK;
}

nput_status nput_list_model_dirs(char **out_lines) {
    if (out_lines == NULL) {
        return NPUT_ERR_INVALID_ARGUMENT;
    }
    *out_lines = NULL;

    nput_posix_candidate *items = NULL;
    size_t count = 0;
    nput_status status = nput_collect_models(&items, &count);
    if (status != NPUT_OK) {
        return status;
    }
    if (count == 0) {
        nput_free_posix_candidates(items, count);
        return NPUT_ERR_UNSUPPORTED;
    }

    size_t total = 1;
    for (size_t i = 0; i < count; ++i) {
        total += strlen(items[i].path) + 1;
    }
    char *lines = (char *)malloc(total);
    if (lines == NULL) {
        nput_free_posix_candidates(items, count);
        return NPUT_ERR_OUT_OF_MEMORY;
    }
    size_t offset = 0;
    for (size_t i = 0; i < count; ++i) {
        const size_t length = strlen(items[i].path);
        memcpy(lines + offset, items[i].path, length);
        offset += length;
        lines[offset++] = (i + 1 == count) ? '\0' : '\n';
    }
    *out_lines = lines;
    nput_free_posix_candidates(items, count);
    return NPUT_OK;
}
#endif
