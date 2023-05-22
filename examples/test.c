#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "threadpool.h"
 
#define NUM_THREADS 4
 
typedef struct {
    double sum;
    int start;
    int end;
} SumArgs;
 
void sum_range(void* arg) {
    SumArgs* args = (SumArgs*)arg;
    double sum = 0.0;
    int i;
    for (i = args->start; i <= args->end; i++) {
        sum += sqrt(i);
    }
    args->sum = sum;
}
 
int main() {
    const int num_tasks = 8;
 
    threadpool* pool = tpool_create(NUM_THREADS);
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        return 1;
    }
    
    SumArgs args_list[num_tasks];
    int i, chunk_size = 1000;

    for (i = 0; i < num_tasks; i++) {
        args_list[i].start = i * chunk_size + 1;
        args_list[i].end = args_list[i].start + chunk_size - 1;
    }

    for (i = 0; i < num_tasks; i++) {
        if (tpool_add_work(pool, sum_range, &args_list[i])) {
            fprintf(stderr, "Failed to add work to thread pool\n");
            return 1;
        }
    }
 
    tpool_wait(pool);
 
    double total_sum = 0.0;
    for (i = 0; i < num_tasks; i++) {
        total_sum += args_list[i].sum;
    }
 
    printf("Total sum: %lf\n", total_sum);
 
    tpool_destroy(pool);
 
    return 0;
}
