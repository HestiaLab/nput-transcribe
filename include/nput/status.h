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
 * @brief 返回当前线程最近一次失败的详细信息。
 *
 * 库在失败时记录描述性信息（例如 OpenVINO 的具体步骤），由调用方决定是否展示。
 * 成功不会自动清空；需要时调用 ::nput_clear_error。
 *
 * @return 以 NUL 结尾的线程局部字符串；没有记录时返回空字符串。
 */
const char *NPUT_API nput_last_error(void);

/** @brief 清空当前线程的错误详情。 */
void NPUT_API nput_clear_error(void);

/**
 * @brief 释放由本库通过 `char **` 输出参数分配的内存。
 * @param pointer 库返回的指针；允许为 NULL。
 */
void NPUT_API nput_free(void *pointer);

#ifdef __cplusplus
}
#endif

#endif
