#include "platform/clock.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

double nput_monotonic_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER now;
    /* 不缓存频率：避免首次调用时的静态变量竞争，便于不同会话并发使用。 */
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec * 1000.0 + (double)now.tv_nsec / 1e6;
#endif
}
