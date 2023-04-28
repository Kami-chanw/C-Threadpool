#ifndef _UTILS_H_
#define _UTILS_H_

#define err(MSG, ...) fprintf(stderr, "File: %s\nLine: %d\nFunction: %s\n" MSG, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#ifdef NDEBUG
#define ASSERT(...) ((__VA_ARGS__) ? (void)0 : unreachable())
#define EXPECT(MSG, ...) ASSERT(__VA_ARGS__)
#else
#define EXPECT(MSG, ...) ((__VA_ARGS__) ? (void)0 : assert_failure(__FILE__, __LINE__, __func__, "assertion failed: " MSG))
#define ASSERT(...) ((__VA_ARGS__) ? (void)0 : assert_failure(__FILE__, __LINE__, __func__, "assertion failed: " #__VA_ARGS__))
#endif

#undef MIN
#undef MAX
#ifdef __GNUC__
#define MIN(a, b)             \
    ({                        \
        __auto_type _a = (a); \
        __auto_type _b = (b); \
        _a < _b ? _a : _b;    \
    })
#define MAX(a, b)             \
    ({                        \
        __auto_type _a = (a); \
        __auto_type _b = (b); \
        _a > _b ? _a : _b;    \
    })
#else
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

// stuffs
void assert_failure(char const* file, int line, char const* func, char const* msg);
void unreachable(void);

// cpu
int cpu_get_num(void);
int cpu_bind_thread(pthread_t t, int core);

// cache
int cache_L1_size(void);
int cache_L1_linesize(void);
int cache_L2_linesize(void);

#ifdef __cplusplus
}
#endif

#endif