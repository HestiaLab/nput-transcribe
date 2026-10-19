#ifndef NPUT_EXPORT_H
#define NPUT_EXPORT_H

/*
 * 公共 API 的导出宏：
 *   - 构建静态库：NPUT_API 为空；
 *   - 构建共享库：库内部定义 NPUT_BUILDING_LIBRARY 导出符号；
 *   - 使用共享库：目标接口定义 NPUT_SHARED，调用方看到 dllimport。
 */
#if defined(_WIN32)
#if defined(NPUT_BUILDING_LIBRARY)
#define NPUT_API __declspec(dllexport)
#elif defined(NPUT_SHARED)
#define NPUT_API __declspec(dllimport)
#else
#define NPUT_API
#endif
#else
#if defined(NPUT_SHARED) && defined(__GNUC__)
#define NPUT_API __attribute__((visibility("default")))
#else
#define NPUT_API
#endif
#endif

#endif
