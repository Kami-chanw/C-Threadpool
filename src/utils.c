#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define __USE_GNU
#include <sched.h>  // cannot change 

#include <pthread.h>

#include "utils.h"
void assert_failure(char const* file, int line, char const* func, char const* msg) {
    fprintf(stderr, "File: %s\nLine: %d\nFunction: %s\n %s\n", file, line, func, msg);
    abort();
}

void unreachable(void) {
#ifdef __GNUC__
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(false);
#endif
}

int cpu_get_num(void) { return sysconf(_SC_NPROCESSORS_ONLN); }

int cpu_bind_thread(pthread_t t, int core) {
    if (core < 0 || core >= cpu_get_num())
        return EINVAL;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    return pthread_setaffinity_np(t, sizeof(cpu_set_t), &cpuset);
}

int cache_L1_size(void) {
    return sysconf(_SC_LEVEL1_DCACHE_SIZE);
}

int cache_L1_linesize(void) {
    return sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
}

int cache_L1_linesize(void) {
    return sysconf(_SC_LEVEL2_DCACHE_LINESIZE);
}