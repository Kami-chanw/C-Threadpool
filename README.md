## C-Threadpool
A small C language thread pool for Linux.

### Structure
In this implementation, I use multiple queues. After new tasks are allocated to the thread pool, they will be randomly and evenly allocated to multiple queues, and then idle threads will take out tasks from them for execution.

The purpose of using multiple queues instead of one is to reduce the wait time for locks. In the future, it is possible to implement multi-level queues (different levels have different priorities).
