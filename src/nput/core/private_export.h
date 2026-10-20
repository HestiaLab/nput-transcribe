#ifndef NPUT_PRIVATE_EXPORT_H
#define NPUT_PRIVATE_EXPORT_H

/*
 * 内部符号的导出宏：仅供同仓库的测试与工具链接。
 * 这些符号不属于公共 API，也不随 include/nput 安装。
 */
#if defined(_WIN32)
#if defined(NPUT_BUILDING_LIBRARY)
#define NPUT_PRIVATE_API __declspec(dllexport)
#elif defined(NPUT_SHARED)
#define NPUT_PRIVATE_API __declspec(dllimport)
#else
#define NPUT_PRIVATE_API
#endif
#else
#if defined(NPUT_SHARED) && defined(__GNUC__)
#define NPUT_PRIVATE_API __attribute__((visibility("default")))
#else
#define NPUT_PRIVATE_API
#endif
#endif

#endif
