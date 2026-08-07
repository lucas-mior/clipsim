// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(THREADS_C)
#define THREADS_C

#include "cbase.h"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_threads 1
#elif !defined(TESTING_threads)
#define TESTING_threads 0
#endif

#if !defined(PARALLEL_FOR_MAX_THREADS)
#define PARALLEL_FOR_MAX_THREADS 64
#endif

#if !defined(MIN_PARALLEL_ITEMS)
#define MIN_PARALLEL_ITEMS 64
#endif

typedef struct ThreadWork {
    ParallelForFunction *function;
    void *user_data;
    int64 start;
    int64 end;
    int32 worker_id;
    int32 padding;
} ThreadWork;

#if OS_WINDOWS
CBASE_API_DEF int32
util_nthreads(void) {
    SYSTEM_INFO sysinfo = {0};
    GetSystemInfo(&sysinfo);
    return (int32)sysinfo.dwNumberOfProcessors;
}
#else
CBASE_API_DEF int32
util_nthreads(void) {
    return (int32)sysconf(_SC_NPROCESSORS_ONLN);
}
#endif

#if OS_UNIX
CBASE_API_DEF void
xpthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr) {
    int err;
    if ((err = pthread_mutex_init(mutex, attr))) {
        error("Error initializing mutex %p: %s.\n",
              (void *)mutex, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
xpthread_mutex_lock(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_lock(mutex))) {
        error("Error locking mutex %p: %s.\n", (void *)mutex, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
xpthread_mutex_unlock(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_unlock(mutex))) {
        error("Error unlocking mutex %p: %s.\n", (void *)mutex, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
xpthread_cond_destroy(pthread_cond_t *cond) {
    int err;
    if ((err = pthread_cond_destroy(cond))) {
        error("Error destroying cond %p: %s.\n", (void *)cond, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
xpthread_mutex_destroy(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_destroy(mutex))) {
        error("Error destroying mutex %p: %s.\n", (void *)mutex, strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
xpthread_create(pthread_t *thread, pthread_attr_t *attr,
                void *(*function)(void *), void *arg) {
    int err;
    if ((err = pthread_create(thread, attr, function, arg))) {
        error("Error creating thread: %s.\n", strerror(err));
        fatal(EXIT_FAILURE);
    }
    return;
}

CBASE_API_DEF void
xpthread_join(pthread_t *thread, void **thread_return) {
    int err;
    if ((err = pthread_join(*thread, thread_return))) {
        error("Error joining thread: %s.\n", strerror(err));
        fatal(EXIT_FAILURE);
    }
    *thread = 0;
    return;
}

CBASE_API_DEF void *
util_copy_file_async_thread(void *arg) {
    UtilCopyFilesAsync *copy_files = arg;
    util_copy_file_async_parsed(copy_files);
    pthread_exit(NULL);
    return NULL;
}
#endif

#if (OS_UNIX || OS_WINDOWS) && (PARALLEL_FOR_MAX_THREADS > 1)
#if OS_UNIX
static pthread_mutex_t thread_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thread_pool_new_work = PTHREAD_COND_INITIALIZER;
static pthread_cond_t thread_pool_done_work = PTHREAD_COND_INITIALIZER;
static pthread_t thread_pool_threads[PARALLEL_FOR_MAX_THREADS];
#elif OS_WINDOWS
static CRITICAL_SECTION thread_pool_mutex;
static CONDITION_VARIABLE thread_pool_new_work;
static CONDITION_VARIABLE thread_pool_done_work;
static HANDLE thread_pool_threads[PARALLEL_FOR_MAX_THREADS];
#endif

static int32 thread_pool_nthreads = 0;
static bool thread_pool_is_stopping = false;
static int32 thread_pool_work_pending = 0;
static bool thread_pool_is_initialized = false;

static struct ThreadWorkQueue {
    ThreadWork *items[PARALLEL_FOR_MAX_THREADS];
    int32 head;
    int32 tail;
    int32 count;
    int32 padding;
} thread_pool_queue = {0};

static void thread_pool_worker_run(void);

#if OS_UNIX
static void
thread_pool_platform_init(void) {
    return;
}

static void
thread_pool_platform_destroy(void) {
    return;
}

static void
thread_pool_lock(void) {
    xpthread_mutex_lock(&thread_pool_mutex);
    return;
}

static void
thread_pool_unlock(void) {
    xpthread_mutex_unlock(&thread_pool_mutex);
    return;
}

static void
thread_pool_wait_new_work(void) {
    pthread_cond_wait(&thread_pool_new_work, &thread_pool_mutex);
    return;
}

static void
thread_pool_wait_done_work(void) {
    pthread_cond_wait(&thread_pool_done_work, &thread_pool_mutex);
    return;
}

static void
thread_pool_signal_new_work(void) {
    pthread_cond_signal(&thread_pool_new_work);
    return;
}

static void
thread_pool_signal_done_work(void) {
    pthread_cond_signal(&thread_pool_done_work);
    return;
}

static void
thread_pool_broadcast_new_work(void) {
    pthread_cond_broadcast(&thread_pool_new_work);
    return;
}

static void *
thread_pool_worker(void *arg) {
    (void)arg;
    thread_pool_worker_run();
    return NULL;
}

static void
thread_pool_start_worker(int32 i) {
    xpthread_create(&thread_pool_threads[i], NULL, thread_pool_worker, NULL);
    return;
}

static void
thread_pool_join_workers(void) {
    for (int32 i = 0; i < thread_pool_nthreads; i += 1) {
        xpthread_join(&thread_pool_threads[i], NULL);
    }
    return;
}
#elif OS_WINDOWS
static void
thread_pool_platform_init(void) {
    InitializeCriticalSection(&thread_pool_mutex);
    InitializeConditionVariable(&thread_pool_new_work);
    InitializeConditionVariable(&thread_pool_done_work);
    return;
}

static void
thread_pool_platform_destroy(void) {
    DeleteCriticalSection(&thread_pool_mutex);
    return;
}

static void
thread_pool_lock(void) {
    EnterCriticalSection(&thread_pool_mutex);
    return;
}

static void
thread_pool_unlock(void) {
    LeaveCriticalSection(&thread_pool_mutex);
    return;
}

static void
thread_pool_wait_condition(CONDITION_VARIABLE *condition) {
    if (!SleepConditionVariableCS(condition, &thread_pool_mutex, INFINITE)) {
        error("Error waiting for condition variable: %lu.\n", GetLastError());
        fatal(EXIT_FAILURE);
    }
    return;
}

static void
thread_pool_wait_new_work(void) {
    thread_pool_wait_condition(&thread_pool_new_work);
    return;
}

static void
thread_pool_wait_done_work(void) {
    thread_pool_wait_condition(&thread_pool_done_work);
    return;
}

static void
thread_pool_signal_new_work(void) {
    WakeConditionVariable(&thread_pool_new_work);
    return;
}

static void
thread_pool_signal_done_work(void) {
    WakeConditionVariable(&thread_pool_done_work);
    return;
}

static void
thread_pool_broadcast_new_work(void) {
    WakeAllConditionVariable(&thread_pool_new_work);
    return;
}

static DWORD WINAPI
thread_pool_worker(void *arg) {
    (void)arg;
    thread_pool_worker_run();
    return 0;
}

static void
thread_pool_start_worker(int32 i) {
    HANDLE thread;

    thread = CreateThread(NULL, 0, thread_pool_worker, NULL, 0, NULL);
    if (thread == NULL) {
        error("Error creating thread: %lu.\n", GetLastError());
        fatal(EXIT_FAILURE);
    }
    thread_pool_threads[i] = thread;
    return;
}

static void
thread_pool_join_workers(void) {
    DWORD wait_result;

    for (int32 i = 0; i < thread_pool_nthreads; i += 1) {
        wait_result = WaitForSingleObject(thread_pool_threads[i], INFINITE);
        if (wait_result == WAIT_FAILED) {
            error("Error joining thread: %lu.\n", GetLastError());
            fatal(EXIT_FAILURE);
        }
        if (!CloseHandle(thread_pool_threads[i])) {
            error("Error closing thread handle: %lu.\n", GetLastError());
            fatal(EXIT_FAILURE);
        }
        thread_pool_threads[i] = NULL;
    }
    return;
}
#endif

static void
thread_pool_shutdown(void) {
    thread_pool_lock();
    thread_pool_is_stopping = true;
    thread_pool_broadcast_new_work();
    thread_pool_unlock();

    thread_pool_join_workers();
    thread_pool_nthreads = 0;
    thread_pool_platform_destroy();
    return;
}

static void
thread_pool_worker_run(void) {
    while (true) {
        ThreadWork *work = NULL;

        thread_pool_lock();
        while ((thread_pool_queue.count <= 0)
               && !thread_pool_is_stopping) {
            thread_pool_wait_new_work();
        }

        if (thread_pool_is_stopping) {
            thread_pool_unlock();
            return;
        }

        if (thread_pool_queue.count > 0) {
            work = thread_pool_queue.items[thread_pool_queue.head];
            thread_pool_queue.items[thread_pool_queue.head] = NULL;
            thread_pool_queue.head = (thread_pool_queue.head + 1)
                                     % LENGTH(thread_pool_queue.items);
            thread_pool_queue.count -= 1;
        }
        thread_pool_unlock();

        if (work) {
            work->function(work->start, work->end, work->worker_id,
                           work->user_data);

            thread_pool_lock();
            thread_pool_work_pending -= 1;
            if ((thread_pool_work_pending <= 0)
                && (thread_pool_queue.count <= 0)) {
                thread_pool_signal_done_work();
            }
            thread_pool_unlock();
        }
    }
}

static int32
thread_pool_default_thread_count(
    int64 length,
    int32 max_threads,
    int64 min_parallel_items
) {
    int32 available_threads;
    int32 thread_count;

    if (length <= 0) {
        return 0;
    }
    if (max_threads <= 1) {
        return 1;
    }
    if (length < min_parallel_items) {
        return 1;
    }

    available_threads = util_nthreads();
    if (available_threads <= 0) {
        available_threads = 1;
    }

    thread_count = (int32)MIN((int64)available_threads,
                             (int64)PARALLEL_FOR_MAX_THREADS);
    thread_count = (int32)MIN((int64)thread_count, (int64)max_threads);
    thread_count = (int32)MIN((int64)thread_count, length);
    if (thread_count < 1) {
        thread_count = 1;
    }
    return thread_count;
}

static void
thread_pool_ensure_started(int32 thread_count) {
    if (thread_count <= thread_pool_nthreads) {
        return;
    }

    if (!thread_pool_is_initialized) {
        thread_pool_is_initialized = true;
        thread_pool_platform_init();
#if CC_GCC || CC_CLANG
        atexit(thread_pool_shutdown);
#endif
    }

    for (int32 i = thread_pool_nthreads; i < thread_count; i += 1) {
        thread_pool_start_worker(i);
    }
    thread_pool_nthreads = thread_count;
    return;
}

CBASE_API_DEF int32
parallel_for_max_threads_min_items(
    int64 length,
    int32 max_threads,
    int64 min_parallel_items,
    ParallelForFunction *function,
    void *user_data
) {
    ThreadWork slices[PARALLEL_FOR_MAX_THREADS];
    int32 thread_count;
    int64 range;

    if (length <= 0) {
        return 0;
    }

    thread_count = thread_pool_default_thread_count(length, max_threads,
                                                   min_parallel_items);
    if (thread_count <= 1) {
        function(0, length, 0, user_data);
        return 1;
    }

    thread_pool_ensure_started(thread_count);
    range = length / thread_count;

    thread_pool_lock();
    for (int32 i = 0; i < thread_count; i += 1) {
        slices[i].function = function;
        slices[i].user_data = user_data;
        slices[i].start = i*range;
        if ((i + 1) < thread_count) {
            slices[i].end = (i + 1)*range;
        } else {
            slices[i].end = length;
        }
        slices[i].worker_id = i;

        if (thread_pool_queue.count >= LENGTH(thread_pool_queue.items)) {
            thread_pool_unlock();
            error("Error: Thread pool work queue is full.\n");
            fatal(EXIT_FAILURE);
        }

        thread_pool_queue.items[thread_pool_queue.tail] = &slices[i];
        thread_pool_queue.tail = (thread_pool_queue.tail + 1)
                                    % LENGTH(thread_pool_queue.items);
        thread_pool_queue.count += 1;
        thread_pool_work_pending += 1;
        thread_pool_signal_new_work();
    }

    while ((thread_pool_work_pending > 0)
           || (thread_pool_queue.count > 0)) {
        thread_pool_wait_done_work();
    }

    for (int32 i = 0; i < LENGTH(thread_pool_queue.items); i += 1) {
        thread_pool_queue.items[i] = NULL;
    }
    thread_pool_unlock();

    return thread_count;
}
#else
CBASE_API_DEF int32
parallel_for_max_threads_min_items(
    int64 length,
    int32 max_threads,
    int64 min_parallel_items,
    ParallelForFunction *function,
    void *user_data
) {
    (void)max_threads;
    (void)min_parallel_items;

    if (length <= 0) {
        return 0;
    }

    function(0, length, 0, user_data);
    return 1;
}
#endif

CBASE_API_DEF int32
parallel_for_min_items(int64 length, int64 min_parallel_items,
                       ParallelForFunction *function, void *user_data) {
    return parallel_for_max_threads_min_items(length, PARALLEL_FOR_MAX_THREADS,
                                             min_parallel_items, function,
                                             user_data);
}

CBASE_API_DEF int32
parallel_for(int64 length, ParallelForFunction *function, void *user_data) {
    return parallel_for_min_items(length, MIN_PARALLEL_ITEMS, function,
                                  user_data);
}

#if 0 == TESTING_threads
static inline void
threads_functions_sink(void) {
    (void)threads_functions_sink;
    (void)util_nthreads;
    (void)parallel_for;
    (void)parallel_for_min_items;
    (void)parallel_for_max_threads_min_items;
#if OS_UNIX
    (void)util_copy_file_async_thread;
    (void)xpthread_mutex_init;
    (void)xpthread_mutex_lock;
    (void)xpthread_mutex_unlock;
    (void)xpthread_cond_destroy;
    (void)xpthread_mutex_destroy;
    (void)xpthread_create;
    (void)xpthread_join;
#endif
    return;
}
#endif

#if TESTING_threads
#define CBASE_IMPLEMENT
#include "cbase.h"

typedef struct ThreadsTestWork {
    int64 *values;
} ThreadsTestWork;

static void
threads_test_fill_work(int64 start, int64 end, int32 worker_id,
                       void *user_data) {
    ThreadsTestWork *work = user_data;

    for (int64 i = start; i < end; i += 1) {
        work->values[i] = i + worker_id;
    }
    return;
}

int32
main(void) {
    int64 length = 10100100;
    int64 *values = malloc2(length*SIZEOF(*values));
    ThreadsTestWork work;
    int32 workers;

    memset64(values, 0, length*SIZEOF(*values));
    work.values = values;
    workers = parallel_for_min_items(length, 1, threads_test_fill_work, &work);

    ASSERT_MORE_EQUAL(workers, 1);
    for (int64 i = 0; i < length; i += 1) {
        ASSERT_MORE_EQUAL(values[i], i);
        ASSERT_LESS(values[i], i + PARALLEL_FOR_MAX_THREADS);
    }

    free2(values, length*SIZEOF(*values));
    exit(EXIT_SUCCESS);
}
#endif /* TESTING_threads */

#endif /* THREADS_C */
