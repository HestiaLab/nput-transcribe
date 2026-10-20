#ifndef NPUT_ERROR_H
#define NPUT_ERROR_H

/**
 * @brief 记录当前线程的错误详情。
 *
 * 仅库内部使用；调用方通过 ::nput_last_error 读取。
 */
void nput_set_error(const char *format, ...);

#endif
