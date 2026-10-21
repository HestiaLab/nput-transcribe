#include "core/error.h"
#include "nput/status.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#define NPUT_THREAD_LOCAL __declspec(thread)
#else
#define NPUT_THREAD_LOCAL _Thread_local
#endif

static NPUT_THREAD_LOCAL char nput_error_message[512];

const char *nput_status_string(nput_status status) {
    switch (status) {
        case NPUT_OK:
            return "成功";
        case NPUT_ERR_INVALID_ARGUMENT:
            return "参数无效";
        case NPUT_ERR_IO:
            return "读写错误";
        case NPUT_ERR_FORMAT:
            return "输入格式错误";
        case NPUT_ERR_OUT_OF_MEMORY:
            return "内存不足";
        case NPUT_ERR_UNSUPPORTED:
            return "不支持";
        case NPUT_ERR_RUNTIME:
            return "运行时错误";
        default:
            return "未知错误";
    }
}

void nput_free(void *pointer) {
    free(pointer);
}

const char *nput_last_error(void) {
    return nput_error_message;
}

void nput_clear_error(void) {
    nput_error_message[0] = '\0';
}

void nput_set_error(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)：va_start 就在上方，LLVM 18 的 analyzer 无法建模
    vsnprintf(nput_error_message, sizeof(nput_error_message), format, arguments);
    va_end(arguments);
}
