#include "nput/status.h"

#include <stdlib.h>

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
