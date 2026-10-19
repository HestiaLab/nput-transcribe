#ifndef NPUT_STATUS_H
#define NPUT_STATUS_H

#include "nput/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 所有公共函数返回的状态码。 */
typedef enum nput_status {
    NPUT_OK = 0,
    NPUT_ERR_INVALID_ARGUMENT,
    NPUT_ERR_IO,
    NPUT_ERR_FORMAT,
    NPUT_ERR_OUT_OF_MEMORY,
    NPUT_ERR_UNSUPPORTED,
    NPUT_ERR_RUNTIME
} nput_status;

/**
 * @brief 返回状态码的简短说明。
 * @param status 要描述的状态码。
 * @return 静态字符串，永不为 NULL。
 */
const char *NPUT_API nput_status_string(nput_status status);

/**
 * @brief 释放由本库通过 `char **` 输出参数分配的内存。
 * @param pointer 库返回的指针；允许为 NULL。
 */
void NPUT_API nput_free(void *pointer);

#ifdef __cplusplus
}
#endif

#endif
