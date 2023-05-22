## Structure

### tpool_pool

```c
typedef struct threadpool threadpool;
```

A struct representing the thread pool.

## Functions

### tpool_create

```c
threadpool* tpool_create(size_t num_threads);
```

This function creates a thread pool with the specified number of threads.

#### Parameters
- `num_threads`: The number of threads to be created in the thread pool.

#### Return Value
A pointer to the created thread pool.

---

### tpool_add_work

```c
int tpool_add_work(threadpool* pool, void (*fcn)(void*), void* arg);
```

This function adds work to the specified thread pool. The work is defined by a function pointer `fcn` and an argument `arg`.

#### Parameters
- `pool`: A pointer to the thread pool.
- `fcn`: A function pointer that represents the work to be executed.
- `arg`: An argument to be passed to the work function.

#### Return Value
- `0` if the work was successfully added to the thread pool.
- `-1` if an error occurred.

---

### tpool_wait

```c
void tpool_wait(threadpool* pool);
```

This function waits for all the work in the thread pool to complete.

#### Parameters
- `pool`: A pointer to the thread pool.

---

### tpool_destroy

```c
void tpool_destroy(threadpool* pool);
```

This function destroys the specified thread pool, freeing the associated resources.

#### Parameters
- `pool`: A pointer to the thread pool.

---

### tpool_num_threads_working

```c
size_t tpool_num_threads_working(struct threadpool* pool);
```

This function returns the number of threads currently working in the thread pool.

#### Parameters
- `pool`: A pointer to the thread pool.

#### Return Value
The number of threads currently working in the thread pool.