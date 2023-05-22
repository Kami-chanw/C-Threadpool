#include "threadpool.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#ifdef __linux__
#include <sys/queue.h>
#include <unistd.h>
#else
#define STAILQ_ENTRY(type)                         \
    struct {                                       \
        struct type* stqe_next; /* next element */ \
    }

#define STAILQ_HEAD(name, type)                                   \
    struct name {                                                 \
        struct type*  stqh_first; /* first element */             \
        struct type** stqh_last;  /* addr of last next element */ \
    }

#define STAILQ_FIRST(head) ((head)->stqh_first)
#define STAILQ_END(head) NULL
#define STAILQ_EMPTY(head) (STAILQ_FIRST(head) == STAILQ_END(head))
#define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)

#define STAILQ_INIT(head)                             \
    do {                                              \
        STAILQ_FIRST((head)) = NULL;                  \
        (head)->stqh_last    = &STAILQ_FIRST((head)); \
    } while (0)

#define STAILQ_INSERT_TAIL(head, elm, field)                    \
    do {                                                        \
        STAILQ_NEXT((elm), field) = NULL;                       \
        *(head)->stqh_last        = (elm);                      \
        (head)->stqh_last         = &STAILQ_NEXT((elm), field); \
    } while (0)

#define STAILQ_REMOVE_HEAD(head, field)                                                \
    do {                                                                               \
        if ((STAILQ_FIRST((head)) = STAILQ_NEXT(STAILQ_FIRST((head)), field)) == NULL) \
            (head)->stqh_last = &STAILQ_FIRST((head));                                 \
    } while (0)
#endif

/* Binary semaphore */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    bool            v;
} bsem_t;

/* Job */
typedef struct node_t {
    void (*function)(void*);   /* function pointer          */
    void* arg;                 /* function's argument       */
    STAILQ_ENTRY(node_t) next; /* pointer to next node_t   */
} node_t;

/* Job queue */
typedef struct {
    pthread_mutex_t rwlock;
    bsem_t          has_jobs; /* flag as binary semaphore  */
    STAILQ_HEAD(stailq_head, node_t) head;
} jobqueue;

/* Thread */
typedef struct {
    size_t      id;
    pthread_t   pthread; /* pointer to actual thread  */
    threadpool* pool;    /* access to tpool          */
} thread;

/* Threadpool */
typedef struct threadpool {
    thread*         threads;             /* pointer to threads        */
    volatile size_t num_threads_alive;   /* threads currently alive   */
    volatile size_t num_threads_working; /* threads currently working */
    volatile bool   threads_keepalive;
    pthread_mutex_t thcount_lock;     /* used for thread count etc */
    pthread_cond_t  threads_all_idle; /* signal to tpool_wait     */

    jobqueue* queues;    /* job queues   */
    size_t    num_queue; /*num of queue */
} threadpool;

static int   thread_init(threadpool* pool, thread* t, size_t id);
static void* thread_loop(thread* t);

static bool all_queue_empty(threadpool* pool);

static int     jobqueue_init(jobqueue* jq);
static void    jobqueue_push(jobqueue* jq, node_t* newjob);
static node_t* jobqueue_pop(jobqueue* jq);
static node_t* jobqueue_steal(jobqueue* jq);
static void    jobqueue_destroy(jobqueue* jq);

static int  bsem_init(bsem_t* bsem, bool value);
static void bsem_destroy(bsem_t* bsem);
static void bsem_post(bsem_t* bsem);
static void bsem_post_all(bsem_t* bsem);
static void bsem_wait(bsem_t* bsem);

threadpool* tpool_create(size_t num_threads) {

    threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
    if (pool == NULL) {
        err("Could not allocate memory for thread pool");
        goto pool_failed;
    }
    pool->threads_keepalive   = true;
    pool->num_threads_alive   = 0;
    pool->num_threads_working = 0;
    pool->num_queue           = MAX((size_t)1, num_threads / cpu_get_num());
    pool->queues              = (jobqueue*)malloc(sizeof(jobqueue) * pool->num_queue);
    size_t i;
    for (i = 0; i < pool->num_queue; ++i)
        if (jobqueue_init(&pool->queues[i]))
            goto jobqueue_failed;

    pool->threads = (thread*)malloc(num_threads * sizeof(thread));
    if (pool->threads == NULL) {
        err("Could not allocate memory for threads");
        goto threads_alloc_failed;
    }

    int ret = pthread_mutex_init(&pool->thcount_lock, NULL);
    if (ret != 0) {
        err("Mutex init failed, err code %d", ret);
        goto mutex_failed;
    }
    ret = pthread_cond_init(&pool->threads_all_idle, NULL);
    if (ret != 0) {
        err("Condition variable init failed, err code %d", ret);
        goto cond_failed;
    }
    size_t n;
    for (n = 0; n < num_threads; n++) {
        if (thread_init(pool, &pool->threads[n], n))
            goto threads_init_failed;
    }
    while (pool->num_threads_alive != num_threads)
        ;

    return pool;

threads_init_failed:
    while (n--)
        pthread_cancel(pool->threads[n].pthread);
    pthread_cond_destroy(&pool->threads_all_idle);
cond_failed:
    pthread_mutex_destroy(&pool->thcount_lock);
mutex_failed:
    free(pool->threads);
threads_alloc_failed:
jobqueue_failed:
    while (i--)
        jobqueue_destroy(&pool->queues[i]);
    free(pool->queues);
    free(pool);
pool_failed:

    return NULL;
}

int tpool_add_work(threadpool* pool, void (*fcn)(void*), void* arg) {
    ASSERT(pool);
    node_t* newjob = (node_t*)malloc(sizeof(node_t));
    if (newjob == NULL) {
        err("Could not allocate memory for new job");
        return -1;
    }
    newjob->function = fcn;
    newjob->arg      = arg;

    unsigned long seed = time(0);
    jobqueue_push(&pool->queues[xorshift_plus32(&seed) % pool->num_queue], newjob);
    return 0;
}

static bool all_queue_empty(threadpool* pool) {
    for (size_t i = 0; i < pool->num_queue; ++i)
        if (!STAILQ_EMPTY(&pool->queues[i].head))
            return false;
    return true;
}

void tpool_wait(threadpool* pool) {
    ASSERT(pool);
    pthread_mutex_lock(&pool->thcount_lock);
    while (pool->num_threads_working || !all_queue_empty(pool))
        pthread_cond_wait(&pool->threads_all_idle, &pool->thcount_lock);
    pthread_mutex_unlock(&pool->thcount_lock);
}

void tpool_destroy(threadpool* pool) {
    if (pool == NULL)
        return;

    pool->threads_keepalive = false;

    while (pool->num_threads_alive) {
        for (size_t i = 0; i < pool->num_queue; ++i)
            bsem_post_all(&pool->queues[i].has_jobs);
    }

    pthread_cond_destroy(&pool->threads_all_idle);
    pthread_mutex_destroy(&pool->thcount_lock);
    free(pool->threads);

    for (size_t i = 0; i < pool->num_queue; ++i)
        jobqueue_destroy(&pool->queues[i]);
    free(pool->queues);
    free(pool);
}

size_t tpool_num_threads_working(threadpool* pool) {
    ASSERT(pool);
    return pool->num_threads_working;
}

static int thread_init(threadpool* pool, thread* t, size_t id) {
    t->pool = pool;
    t->id   = id;

    int ret = pthread_create(&t->pthread, NULL, (void* (*)(void*))thread_loop, t);
    if (ret) {
        err("pthread create failed, err code %d", ret);
        goto thread_failed;
    }
    ret = cpu_bind_thread(t->pthread, id % cpu_get_num());
    if (ret) {
        err("set cpu affinity for thread failed, err code %d", ret);
        goto failed;
    }
    ret = pthread_detach(t->pthread);
    if (ret) {
        err("pthread detach failed, err code %d", ret);
        goto failed;
    }

    return 0;

failed:
    pthread_cancel(t->pthread);
thread_failed:
    return -1;
}

static void* thread_loop(thread* t) {
    threadpool* pool = t->pool;
    pthread_mutex_lock(&pool->thcount_lock);
    ++pool->num_threads_alive;
    pthread_mutex_unlock(&pool->thcount_lock);
    size_t index = t->id % pool->num_queue;
    while (pool->threads_keepalive) {
        bsem_wait(&pool->queues[index].has_jobs);

        if (pool->threads_keepalive) {
            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_threads_working++;
            pthread_mutex_unlock(&pool->thcount_lock);

            node_t* job = jobqueue_pop(&pool->queues[index]);
            if (job == NULL) {
                for (size_t i = 0; i < pool->num_queue; ++i)
                    if (i != index) {
                        job = jobqueue_steal(&pool->queues[i]);
                        if (job) {
                            job->function(job->arg);
                            free(job);
                            break;
                        }
                    }
            }
            else {
                job->function(job->arg);
                free(job);
            }

            pthread_mutex_lock(&pool->thcount_lock);
            pool->num_threads_working--;
            if (!pool->num_threads_working)
                pthread_cond_signal(&pool->threads_all_idle);
            pthread_mutex_unlock(&pool->thcount_lock);
        }
    }
    pthread_mutex_lock(&pool->thcount_lock);
    pool->num_threads_alive--;
    pthread_mutex_unlock(&pool->thcount_lock);

    return NULL;
}

static int jobqueue_init(jobqueue* jq) {
    STAILQ_INIT(&jq->head);
    int ret = pthread_mutex_init(&jq->rwlock, NULL);
    if (ret) {
        err("Lock init failed, err code %d", ret);
        goto lock_failed;
    }
    if (bsem_init(&jq->has_jobs, 0))
        goto job_sem_failed;
    return 0;

job_sem_failed:
    pthread_mutex_destroy(&jq->rwlock);
lock_failed:
    return -1;
}

static void jobqueue_push(jobqueue* jq, node_t* newjob) {
    pthread_mutex_lock(&jq->rwlock);
    STAILQ_INSERT_TAIL(&jq->head, newjob, next);
    bsem_post(&jq->has_jobs);
    pthread_mutex_unlock(&jq->rwlock);
}

static node_t* jobqueue_pop(jobqueue* jq) {
    pthread_mutex_lock(&jq->rwlock);
    if (STAILQ_EMPTY(&jq->head)) {
        pthread_mutex_unlock(&jq->rwlock);
        return NULL;
    }
    node_t* job = STAILQ_FIRST(&jq->head);
    STAILQ_REMOVE_HEAD(&jq->head, next);
    if (!STAILQ_EMPTY(&jq->head))
        bsem_post(&jq->has_jobs);

    pthread_mutex_unlock(&jq->rwlock);
    return job;
}

static node_t* jobqueue_steal(jobqueue* jq) {
    node_t* job = NULL;
    int     ret = pthread_mutex_trylock(&jq->rwlock);
    if (ret)
        goto lock_failed;
    if (STAILQ_EMPTY(&jq->head))
        goto empty_queue;

    job = STAILQ_FIRST(&jq->head);
    STAILQ_REMOVE_HEAD(&jq->head, next);
    if (!STAILQ_EMPTY(&jq->head))
        bsem_post(&jq->has_jobs);
empty_queue:
    pthread_mutex_unlock(&jq->rwlock);
lock_failed:
    return job;
}

static void jobqueue_destroy(jobqueue* jq) {
    while (!STAILQ_EMPTY(&jq->head))
        free(jobqueue_pop(jq));
    bsem_destroy(&jq->has_jobs);
}

static int bsem_init(bsem_t* bsem, bool value) {
    EXPECT("Binary semaphore can take only values 1 or 0", value == 0 || value == 1);
    int ret = pthread_mutex_init(&bsem->mutex, NULL);
    if (ret) {
        err("Mutex of binary semaphore init failed, err code %d", ret);
        goto mutex_failed;
    }
    ret = pthread_cond_init(&bsem->cond, NULL);
    if (ret) {
        err("Conditional variable of binary semaphore init failed, err code %d", ret);
        goto cond_failed;
    }
    bsem->v = value;
    return 0;
cond_failed:
    pthread_mutex_destroy(&bsem->mutex);
mutex_failed:
    return -1;
}

static void bsem_destroy(bsem_t* bsem) {
    bsem->v = false;
    pthread_mutex_destroy(&(bsem->mutex));
    pthread_cond_destroy(&(bsem->cond));
}

static void bsem_post(bsem_t* bsem) {
    pthread_mutex_lock(&bsem->mutex);
    bsem->v = true;
    pthread_cond_signal(&bsem->cond);
    pthread_mutex_unlock(&bsem->mutex);
}

static void bsem_post_all(bsem_t* bsem) {
    pthread_mutex_lock(&bsem->mutex);
    bsem->v = true;
    pthread_cond_broadcast(&bsem->cond);
    pthread_mutex_unlock(&bsem->mutex);
}

static void bsem_wait(bsem_t* bsem) {
    pthread_mutex_lock(&bsem->mutex);
    while (bsem->v)
        pthread_cond_wait(&bsem->cond, &bsem->mutex);
    bsem->v = false;
    pthread_mutex_unlock(&bsem->mutex);
}
