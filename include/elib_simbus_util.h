/* elib_simbus_util.h - SimBus Utility Macros */

#ifndef ELIB_SIMBUS_UTIL_H
#define ELIB_SIMBUS_UTIL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__ICCARM__)
    #define ELIB_SIMBUS_WEAK __weak
    #define ELIB_SIMBUS_UNUSED __attribute__((unused))
#elif defined(__CC_ARM)
    #define ELIB_SIMBUS_WEAK __weak
    #define ELIB_SIMBUS_UNUSED __attribute__((unused))
#elif defined(__GNUC__)
    #define ELIB_SIMBUS_WEAK __attribute__((weak))
    #define ELIB_SIMBUS_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)
    #define ELIB_SIMBUS_WEAK
    #define ELIB_SIMBUS_UNUSED __pragma(warning(suppress:4100))
#else
    #define ELIB_SIMBUS_WEAK
    #define ELIB_SIMBUS_UNUSED
#endif

#define ELIB_SIMBUS_MIN(a, b)  ((a) < (b) ? (a) : (b))
#define ELIB_SIMBUS_MAX(a, b)  ((a) > (b) ? (a) : (b))
#define ELIB_SIMBUS_CLAMP(val, lo, hi)  ELIB_SIMBUS_MIN(ELIB_SIMBUS_MAX(val, lo), hi)

#define ELIB_SIMBUS_ARRAY_SIZE(arr) ((uint32_t)(sizeof(arr) / sizeof((arr)[0])))
#define ELIB_SIMBUS_BIT(n) (1U << (n))

#define ELIB_SIMBUS_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - (char *)(&((type *)0)->member)))

#ifdef __cplusplus
}
#endif

#endif /* ELIB_SIMBUS_UTIL_H */
