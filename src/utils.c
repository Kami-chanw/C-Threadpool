#define _GNU_SOURCE
#ifdef __linux__
#include <sched.h>  // cannot change include order
#include <unistd.h>

#include <pthread.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

int cpu_bind_thread(pthread_t t, int core) {
    if (core < 0 || core >= cpu_get_num())
        return EINVAL;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    return pthread_setaffinity_np(t, sizeof(cpu_set_t), &cpuset);
}
static cache_size_t L1_size     = 0;
static cache_size_t L1_linesize = 0;
static cache_size_t L2_linesize = 0;
static size_t       cpu_num     = 0;

#ifdef _WIN32
static void init_cache_size(void) {
    DWORD                                 buffer_size = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer      = NULL;

    GetLogicalProcessorInformation(NULL, &buffer_size);
    buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(buffer_size);
    ASSERT(buffer);
    GetLogicalProcessorInformation(buffer, &buffer_size);

    for (DWORD offset = 0; offset < buffer_size; offset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = &buffer[offset / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)];
        if (ptr->Relationship == RelationCache) {
            if (ptr->Cache.Level == 1) {
                L1_size     = (cache_size_t)ptr->Cache.Size;
                L1_linesize = (cache_size_t)ptr->Cache.LineSize;
            }
            else if (ptr->Cache.Level == 2)
                L2_linesize = (cache_size_t)ptr->Cache.LineSize;
        }
    }
    free(buffer);
}
#endif

int cpu_get_num(void) {
    if (cpu_num == 0) {
#ifdef __linux__
        cpu_num = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined _WIN32
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        cpu_num = sys_info.dwNumberOfProcessors;
#endif
    }
    return cpu_num;
}

cache_size_t cache_L1_size(void) {
    if (L1_size == 0) {
#ifdef __linux__
        cpu_num = sysconf(_SC_LEVEL1_DCACHE_SIZE);
#elif defined _WIN32
        init_cache_size();
#endif
    }
    return L1_size;
}

cache_size_t cache_L1_linesize(void) {
    if (L1_linesize == 0) {
#ifdef __linux__
        cpu_num = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
#elif defined _WIN32
        init_cache_size();
#endif
    }
    return L1_linesize;
}

cache_size_t cache_L2_linesize(void) {
    if (L2_linesize == 0) {
#ifdef __linux__
        cpu_num = sysconf(_SC_LEVEL2_CACHE_LINESIZE);
#elif defined _WIN32
        init_cache_size();
#endif
    }
    return L2_linesize;
}

unsigned long xorshift_plus32(unsigned long* seed) {
    unsigned long s = *seed;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *seed = s;
    return s;
}