#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
struct threadpool;
typedef struct threadpool threadpool;

threadpool* tpool_create(int num_threads);
int         tpool_add_work(threadpool*, void (*fcn)(void*), void* arg);
void        tpool_wait(threadpool*);
void        tpool_destroy(threadpool*);
size_t      tpool_num_threads_working(struct threadpool*);

#ifdef __cplusplus
}
#endif

#endif